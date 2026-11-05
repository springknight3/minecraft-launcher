package net.stratalauncher.core.download;

import com.sun.net.httpserver.HttpServer;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class DownloadManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void downloadsConcurrentlyVerifiesChecksumsAndRetriesTransientFailures() throws Exception {
        byte[] content = "verified game file".getBytes(StandardCharsets.UTF_8);
        String sha1 = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-1").digest(content));
        AtomicInteger retryRequests = new AtomicInteger();
        AtomicInteger activeRequests = new AtomicInteger();
        AtomicInteger maxConcurrentRequests = new AtomicInteger();
        CountDownLatch concurrentRequestsStarted = new CountDownLatch(2);
        ExecutorService serverExecutor = Executors.newCachedThreadPool();
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.setExecutor(serverExecutor);
        server.createContext("/retry", exchange -> {
            concurrentRequestsStarted.countDown();
            if (!awaitConcurrentRequests(concurrentRequestsStarted)) {
                exchange.sendResponseHeaders(504, -1);
                exchange.close();
                return;
            }
            int active = activeRequests.incrementAndGet();
            maxConcurrentRequests.accumulateAndGet(active, Math::max);
            if (retryRequests.getAndIncrement() == 0) {
                exchange.sendResponseHeaders(503, -1);
            } else {
                exchange.sendResponseHeaders(200, content.length);
                exchange.getResponseBody().write(content);
            }
            activeRequests.decrementAndGet();
            exchange.close();
        });
        server.createContext("/file", exchange -> {
            concurrentRequestsStarted.countDown();
            if (!awaitConcurrentRequests(concurrentRequestsStarted)) {
                exchange.sendResponseHeaders(504, -1);
                exchange.close();
                return;
            }
            int active = activeRequests.incrementAndGet();
            maxConcurrentRequests.accumulateAndGet(active, Math::max);
            exchange.sendResponseHeaders(200, content.length);
            exchange.getResponseBody().write(content);
            activeRequests.decrementAndGet();
            exchange.close();
        });
        server.start();

        try (DownloadManager manager = new DownloadManager(2, 3)) {
            int port = server.getAddress().getPort();
            List<DownloadManager.Progress> progress = new CopyOnWriteArrayList<>();
            Path retriedFile = temporaryDirectory.resolve("retry/file.jar");
            Path regularFile = temporaryDirectory.resolve("regular/file.jar");
            manager.downloadAll(List.of(
                    new DownloadTask(serverUri(port, "/retry"), retriedFile, sha1, "retry file"),
                    new DownloadTask(serverUri(port, "/file"), regularFile, sha1, "regular file")), progress::add);

            assertEquals(2, retryRequests.get());
            assertEquals("verified game file", Files.readString(retriedFile));
            assertEquals("verified game file", Files.readString(regularFile));
            assertTrue(maxConcurrentRequests.get() >= 2);
            assertTrue(progress.stream().anyMatch(update -> update.completed() == 2 && update.total() == 2));
        } finally {
            server.stop(0);
            serverExecutor.shutdownNow();
        }
    }

    @Test
    void doesNotLeaveFileWhenChecksumCannotBeVerified() throws Exception {
        byte[] content = "wrong contents".getBytes(StandardCharsets.UTF_8);
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/file", exchange -> {
            exchange.sendResponseHeaders(200, content.length);
            exchange.getResponseBody().write(content);
            exchange.close();
        });
        server.start();

        Path destination = temporaryDirectory.resolve("invalid/file.jar");
        try (DownloadManager manager = new DownloadManager(1, 2)) {
            DownloadTask task = new DownloadTask(serverUri(server.getAddress().getPort(), "/file"),
                    destination, "0000000000000000000000000000000000000000", "invalid file");

            assertThrows(IOException.class, () -> manager.downloadAll(List.of(task), ignored -> { }));
            assertTrue(Files.notExists(destination));
            try (var files = Files.list(destination.getParent())) {
                assertEquals(0, files.count());
            }
        } finally {
            server.stop(0);
        }
    }

    private static java.net.URI serverUri(int port, String path) {
        return java.net.URI.create("http://127.0.0.1:" + port + path);
    }

    private static boolean awaitConcurrentRequests(CountDownLatch latch) throws IOException {
        try {
            return latch.await(3, TimeUnit.SECONDS);
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted while testing concurrent downloads", exception);
        }
    }
}
