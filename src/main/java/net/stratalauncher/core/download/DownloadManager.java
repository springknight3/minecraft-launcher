package net.stratalauncher.core.download;

import java.io.IOException;
import java.io.InputStream;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Duration;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CompletionService;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorCompletionService;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

public final class DownloadManager implements AutoCloseable {
    private static final int DEFAULT_CONCURRENCY = 6;
    private static final int DEFAULT_RETRIES = 3;
    private static final long RETRY_DELAY_MILLIS = 250;

    private final HttpClient httpClient;
    private final ExecutorService executor;
    private final int retries;

    public DownloadManager() {
        this(DEFAULT_CONCURRENCY, DEFAULT_RETRIES);
    }

    public DownloadManager(int concurrency, int retries) {
        if (concurrency < 1) {
            throw new IllegalArgumentException("Download concurrency must be at least one");
        }
        if (retries < 1) {
            throw new IllegalArgumentException("Download retries must be at least one");
        }
        this.retries = retries;
        httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(15))
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
        executor = Executors.newFixedThreadPool(concurrency, runnable -> {
            Thread thread = new Thread(runnable, "stratalauncher-download");
            thread.setDaemon(true);
            return thread;
        });
    }

    public void downloadAll(List<DownloadTask> tasks, Consumer<Progress> progress)
            throws IOException, InterruptedException {
        Objects.requireNonNull(tasks, "tasks");
        Consumer<Progress> listener = progress == null ? ignored -> { } : progress;
        List<DownloadTask> work = List.copyOf(tasks);
        ensureUniqueDestinations(work);
        if (work.isEmpty()) {
            listener.accept(new Progress(0, 0, "Nothing to download"));
            return;
        }

        AtomicInteger completed = new AtomicInteger();
        Object progressLock = new Object();
        CompletionService<Void> completion = new ExecutorCompletionService<>(executor);
        List<java.util.concurrent.Future<Void>> futures = new ArrayList<>(work.size());
        for (DownloadTask task : work) {
            futures.add(completion.submit(() -> {
                synchronized (progressLock) {
                    listener.accept(new Progress(completed.get(), work.size(), task.description()));
                }
                downloadWithRetries(task);
                synchronized (progressLock) {
                    int count = completed.incrementAndGet();
                    listener.accept(new Progress(count, work.size(), task.description()));
                }
                return null;
            }));
        }

        try {
            for (int i = 0; i < work.size(); i++) {
                completion.take().get();
            }
        } catch (ExecutionException exception) {
            cancel(futures);
            Throwable cause = exception.getCause();
            if (cause instanceof IOException ioException) {
                throw ioException;
            }
            throw new IOException("Download task failed", cause);
        } catch (InterruptedException exception) {
            cancel(futures);
            Thread.currentThread().interrupt();
            throw exception;
        }
    }

    private void downloadWithRetries(DownloadTask task) throws IOException, InterruptedException {
        Path destination = task.normalizedDestination();
        Files.createDirectories(destination.getParent());
        if (isValidExistingFile(destination, task.expectedSha1())) {
            return;
        }
        Files.deleteIfExists(destination);

        IOException lastFailure = null;
        for (int attempt = 1; attempt <= retries; attempt++) {
            Path temporaryFile = Files.createTempFile(destination.getParent(), ".download-", ".tmp");
            try {
                downloadAttempt(task, temporaryFile);
                verifyChecksum(temporaryFile, task.expectedSha1(), task.source());
                moveIntoPlace(temporaryFile, destination);
                return;
            } catch (IOException exception) {
                lastFailure = exception;
                Files.deleteIfExists(temporaryFile);
                if (attempt < retries) {
                    try {
                        Thread.sleep(RETRY_DELAY_MILLIS * attempt);
                    } catch (InterruptedException interruptedException) {
                        Thread.currentThread().interrupt();
                        throw interruptedException;
                    }
                }
            } finally {
                Files.deleteIfExists(temporaryFile);
            }
        }
        throw new IOException("Download failed after " + retries + " attempts: " + task.source(), lastFailure);
    }

    private void downloadAttempt(DownloadTask task, Path temporaryFile)
            throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(task.source())
                .timeout(Duration.ofMinutes(3))
                .header("Accept", "*/*")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET()
                .build();
        HttpResponse<InputStream> response = httpClient.send(request, HttpResponse.BodyHandlers.ofInputStream());
        try (InputStream input = response.body()) {
            if (response.statusCode() < 200 || response.statusCode() >= 300) {
                throw new IOException("Server returned HTTP " + response.statusCode() + " for " + task.source());
            }
            Files.copy(input, temporaryFile, StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private static boolean isValidExistingFile(Path file, String expectedSha1) throws IOException {
        if (!Files.isRegularFile(file)) {
            return false;
        }
        return expectedSha1.isEmpty() || expectedSha1.equalsIgnoreCase(sha1(file));
    }

    private static void verifyChecksum(Path file, String expectedSha1, java.net.URI source) throws IOException {
        if (!expectedSha1.isEmpty() && !expectedSha1.equalsIgnoreCase(sha1(file))) {
            throw new IOException("SHA-1 checksum mismatch for " + source);
        }
    }

    private static String sha1(Path file) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-1");
            try (InputStream input = Files.newInputStream(file)) {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    digest.update(buffer, 0, count);
                }
            }
            return HexFormat.of().formatHex(digest.digest());
        } catch (NoSuchAlgorithmException exception) {
            throw new IllegalStateException("SHA-1 is not available", exception);
        }
    }

    private static void moveIntoPlace(Path temporaryFile, Path destination) throws IOException {
        try {
            Files.move(temporaryFile, destination, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        } catch (AtomicMoveNotSupportedException exception) {
            Files.move(temporaryFile, destination, StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private static void ensureUniqueDestinations(List<DownloadTask> tasks) {
        Set<Path> destinations = new HashSet<>();
        for (DownloadTask task : tasks) {
            if (!destinations.add(task.normalizedDestination())) {
                throw new IllegalArgumentException("Multiple download tasks target " + task.normalizedDestination());
            }
        }
    }

    private static void cancel(List<java.util.concurrent.Future<Void>> futures) {
        futures.forEach(future -> future.cancel(true));
    }

    @Override
    public void close() {
        executor.shutdown();
        try {
            if (!executor.awaitTermination(3, TimeUnit.SECONDS)) {
                executor.shutdownNow();
            }
        } catch (InterruptedException exception) {
            executor.shutdownNow();
            Thread.currentThread().interrupt();
        }
    }

    public record Progress(int completed, int total, String currentItem) {
        public Progress {
            if (completed < 0 || total < 0 || completed > total) {
                throw new IllegalArgumentException("Invalid download progress");
            }
            Objects.requireNonNull(currentItem, "currentItem");
        }

        public double fraction() {
            return total == 0 ? 1 : (double) completed / total;
        }
    }
}
