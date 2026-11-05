package net.stratalauncher.core.settings;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.LinkedHashMap;
import java.util.Map;

public final class OptionsParser {
    private OptionsParser() {
    }

    public static Map<String, String> read(Path file) throws IOException {
        if (!Files.isRegularFile(file)) {
            return Map.of();
        }
        return parse(Files.readString(file));
    }

    public static Map<String, String> parse(String content) {
        Map<String, String> values = new LinkedHashMap<>();
        for (String line : content.split("\\R", -1)) {
            int separator = line.indexOf(':');
            if (separator > 0) {
                values.put(line.substring(0, separator), line.substring(separator + 1));
            }
        }
        return Map.copyOf(values);
    }

    public static void set(Path file, String key, String value) throws IOException {
        setAll(file, Map.of(key, value));
    }

    public static void setAll(Path file, Map<String, String> changes) throws IOException {
        String content = Files.isRegularFile(file) ? Files.readString(file) : "";
        for (Map.Entry<String, String> change : changes.entrySet()) {
            content = update(content, change.getKey(), change.getValue());
        }
        write(file, content);
    }

    public static String update(String content, String key, String value) {
        validateKey(key);
        if (value == null || value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0) {
            throw new IllegalArgumentException("Option values must be a single line.");
        }

        int matchStart = -1;
        int matchEnd = -1;
        int matchSeparator = -1;
        int lineStart = 0;
        while (lineStart < content.length()) {
            int lineBreak = nextLineBreak(content, lineStart);
            int lineEndingLength = lineBreak < 0 ? 0
                    : content.charAt(lineBreak) == '\r' && lineBreak + 1 < content.length()
                    && content.charAt(lineBreak + 1) == '\n' ? 2 : 1;
            int lineEnd = lineBreak < 0 ? content.length() : lineBreak + lineEndingLength;
            int textEnd = lineBreak < 0 ? lineEnd : lineBreak;
            int separator = content.indexOf(':', lineStart);
            if (separator >= lineStart && separator < textEnd
                    && content.substring(lineStart, separator).equals(key)) {
                matchStart = lineStart;
                matchEnd = lineEnd;
                matchSeparator = separator;
            }
            lineStart = lineEnd;
        }

        if (matchStart >= 0) {
            int lineBreak = nextLineBreak(content, matchStart);
            String lineEnding = lineBreak < 0 ? "" : content.substring(lineBreak, matchEnd);
            return content.substring(0, matchSeparator + 1) + value + lineEnding
                    + content.substring(matchEnd);
        }

        String newline = firstLineEnding(content);
        String prefix = content.isEmpty() || endsWithLineBreak(content) ? content : content + newline;
        return prefix + key + ":" + value + newline;
    }

    private static void write(Path file, String content) throws IOException {
        Path destination = file.toAbsolutePath().normalize();
        Files.createDirectories(destination.getParent());
        Path temporary = Files.createTempFile(destination.getParent(), "options-", ".tmp");
        try {
            Files.writeString(temporary, content);
            try {
                Files.move(temporary, destination, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, destination, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private static void validateKey(String key) {
        if (key == null || key.isBlank() || key.indexOf(':') >= 0
                || key.indexOf('\n') >= 0 || key.indexOf('\r') >= 0) {
            throw new IllegalArgumentException("Option keys must be non-empty and cannot contain colons or line breaks.");
        }
    }

    private static int nextLineBreak(String content, int fromIndex) {
        int lf = content.indexOf('\n', fromIndex);
        int cr = content.indexOf('\r', fromIndex);
        if (lf < 0) {
            return cr;
        }
        return cr < 0 ? lf : Math.min(lf, cr);
    }

    private static String firstLineEnding(String content) {
        int index = nextLineBreak(content, 0);
        if (index < 0) {
            return "\n";
        }
        return content.charAt(index) == '\r' && index + 1 < content.length()
                && content.charAt(index + 1) == '\n' ? "\r\n" : content.substring(index, index + 1);
    }

    private static boolean endsWithLineBreak(String content) {
        return content.endsWith("\n") || content.endsWith("\r");
    }
}
