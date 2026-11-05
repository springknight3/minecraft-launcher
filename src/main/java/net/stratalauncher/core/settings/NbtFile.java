package net.stratalauncher.core.settings;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PushbackInputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

final class NbtFile {
    private static final int MAX_DEPTH = 64;
    private static final int MAX_COLLECTION_SIZE = 2_000_000;

    private NbtFile() {
    }

    static Document read(Path file) throws IOException {
        PushbackInputStream probe = new PushbackInputStream(Files.newInputStream(file), 2);
        byte[] signature = probe.readNBytes(2);
        probe.unread(signature);
        InputStream content = isGzipSignature(signature) ? new GZIPInputStream(probe) : probe;
        try (DataInputStream input = new DataInputStream(content)) {
            int rootType = input.readUnsignedByte();
            if (rootType != 10) {
                throw new IOException("servers.dat root must be an NBT compound.");
            }
            String rootName = input.readUTF();
            Tag root = readPayload(input, (byte) rootType, 0);
            return new Document(rootName, compoundValue(root));
        } catch (IllegalArgumentException exception) {
            throw new IOException("servers.dat contains invalid NBT data.", exception);
        }
    }

    static void write(Path file, Document document, boolean compressed) throws IOException {
        OutputStream fileStream = Files.newOutputStream(file);
        OutputStream content = compressed ? new GZIPOutputStream(fileStream) : fileStream;
        try (DataOutputStream output = new DataOutputStream(content)) {
            output.writeByte(10);
            output.writeUTF(document.rootName);
            writePayload(output, new Tag((byte) 10, document.root), 0);
        }
    }

    static boolean isCompressed(Path file) throws IOException {
        try (InputStream input = Files.newInputStream(file)) {
            return isGzipSignature(input.readNBytes(2));
        }
    }

    private static boolean isGzipSignature(byte[] signature) {
        return signature.length == 2 && signature[0] == (byte) 0x1f && signature[1] == (byte) 0x8b;
    }

    private static Tag readPayload(DataInputStream input, byte type, int depth) throws IOException {
        if (depth > MAX_DEPTH) {
            throw new IOException("NBT nesting is too deep.");
        }
        return switch (type) {
            case 1 -> new Tag(type, input.readByte());
            case 2 -> new Tag(type, input.readShort());
            case 3 -> new Tag(type, input.readInt());
            case 4 -> new Tag(type, input.readLong());
            case 5 -> new Tag(type, input.readFloat());
            case 6 -> new Tag(type, input.readDouble());
            case 7 -> new Tag(type, readBytes(input));
            case 8 -> new Tag(type, input.readUTF());
            case 9 -> readList(input, depth);
            case 10 -> readCompound(input, depth);
            case 11 -> new Tag(type, readInts(input));
            case 12 -> new Tag(type, readLongs(input));
            default -> throw new IOException("Unsupported NBT tag type: " + type);
        };
    }

    private static Tag readList(DataInputStream input, int depth) throws IOException {
        byte elementType = input.readByte();
        int size = readSize(input);
        if (elementType == 0 && size != 0) {
            throw new IOException("Non-empty NBT lists cannot contain end tags.");
        }
        List<Tag> elements = new ArrayList<>(size);
        for (int index = 0; index < size; index++) {
            elements.add(readPayload(input, elementType, depth + 1));
        }
        return new Tag((byte) 9, new ListValue(elementType, elements));
    }

    private static Tag readCompound(DataInputStream input, int depth) throws IOException {
        Map<String, Tag> values = new LinkedHashMap<>();
        while (true) {
            byte type = input.readByte();
            if (type == 0) {
                return new Tag((byte) 10, values);
            }
            if (type < 1 || type > 12 || values.size() >= MAX_COLLECTION_SIZE) {
                throw new IOException("Invalid or oversized NBT compound.");
            }
            values.put(input.readUTF(), readPayload(input, type, depth + 1));
        }
    }

    private static byte[] readBytes(DataInputStream input) throws IOException {
        int size = readSize(input);
        byte[] values = new byte[size];
        input.readFully(values);
        return values;
    }

    private static int[] readInts(DataInputStream input) throws IOException {
        int size = readSize(input);
        int[] values = new int[size];
        for (int index = 0; index < size; index++) {
            values[index] = input.readInt();
        }
        return values;
    }

    private static long[] readLongs(DataInputStream input) throws IOException {
        int size = readSize(input);
        long[] values = new long[size];
        for (int index = 0; index < size; index++) {
            values[index] = input.readLong();
        }
        return values;
    }

    private static int readSize(DataInputStream input) throws IOException {
        int size = input.readInt();
        if (size < 0 || size > MAX_COLLECTION_SIZE) {
            throw new IOException("Invalid or oversized NBT collection.");
        }
        return size;
    }

    @SuppressWarnings("unchecked")
    private static Map<String, Tag> compoundValue(Tag tag) throws IOException {
        if (tag.type != 10 || !(tag.value instanceof Map<?, ?>)) {
            throw new IOException("Expected an NBT compound.");
        }
        return (Map<String, Tag>) tag.value;
    }

    private static void writePayload(DataOutputStream output, Tag tag, int depth) throws IOException {
        if (depth > MAX_DEPTH) {
            throw new IOException("NBT nesting is too deep.");
        }
        switch (tag.type) {
            case 1 -> output.writeByte((Byte) tag.value);
            case 2 -> output.writeShort((Short) tag.value);
            case 3 -> output.writeInt((Integer) tag.value);
            case 4 -> output.writeLong((Long) tag.value);
            case 5 -> output.writeFloat((Float) tag.value);
            case 6 -> output.writeDouble((Double) tag.value);
            case 7 -> {
                byte[] values = (byte[]) tag.value;
                output.writeInt(values.length);
                output.write(values);
            }
            case 8 -> output.writeUTF((String) tag.value);
            case 9 -> writeList(output, (ListValue) tag.value, depth);
            case 10 -> writeCompound(output, compoundValue(tag), depth);
            case 11 -> {
                int[] values = (int[]) tag.value;
                output.writeInt(values.length);
                for (int value : values) {
                    output.writeInt(value);
                }
            }
            case 12 -> {
                long[] values = (long[]) tag.value;
                output.writeInt(values.length);
                for (long value : values) {
                    output.writeLong(value);
                }
            }
            default -> throw new IOException("Unsupported NBT tag type: " + tag.type);
        }
    }

    private static void writeList(DataOutputStream output, ListValue list, int depth) throws IOException {
        output.writeByte(list.elementType);
        output.writeInt(list.elements.size());
        for (Tag element : list.elements) {
            if (element.type != list.elementType) {
                throw new IOException("NBT list elements have inconsistent types.");
            }
            writePayload(output, element, depth + 1);
        }
    }

    private static void writeCompound(DataOutputStream output, Map<String, Tag> values, int depth)
            throws IOException {
        for (Map.Entry<String, Tag> entry : values.entrySet()) {
            output.writeByte(entry.getValue().type);
            output.writeUTF(entry.getKey());
            writePayload(output, entry.getValue(), depth + 1);
        }
        output.writeByte(0);
    }

    record Tag(byte type, Object value) {
    }

    record ListValue(byte elementType, List<Tag> elements) {
    }

    record Document(String rootName, Map<String, Tag> root) {
    }
}
