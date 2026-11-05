package net.stratalauncher.core.loader;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

final class MavenVersions {
    private static final Pattern SAFE_VERSION = Pattern.compile("[A-Za-z0-9._+~-]+");

    private MavenVersions() {
    }

    static List<String> parse(String xml, String prefix) throws IOException {
        try {
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            factory.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, true);
            factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true);
            factory.setXIncludeAware(false);
            factory.setExpandEntityReferences(false);
            var builder = factory.newDocumentBuilder();
            var document = builder.parse(new InputSource(new StringReader(xml)));
            NodeList entries = document.getElementsByTagName("version");
            List<String> versions = new ArrayList<>();
            for (int index = 0; index < entries.getLength(); index++) {
                if (!(entries.item(index) instanceof Element entry)) {
                    continue;
                }
                String version = entry.getTextContent().trim();
                if (version.startsWith(prefix) && SAFE_VERSION.matcher(version).matches()
                        && !versions.contains(version)) {
                    versions.add(version);
                }
            }
            versions.sort((left, right) -> compareVersions(right, left));
            return List.copyOf(versions);
        } catch (SAXException | javax.xml.parsers.ParserConfigurationException exception) {
            throw new IOException("Loader Maven metadata is invalid.", exception);
        }
    }

    private static int compareVersions(String left, String right) {
        String[] leftParts = left.split("[.-]");
        String[] rightParts = right.split("[.-]");
        for (int index = 0; index < Math.min(leftParts.length, rightParts.length); index++) {
            int comparison;
            try {
                comparison = Long.compare(Long.parseLong(leftParts[index]), Long.parseLong(rightParts[index]));
            } catch (NumberFormatException exception) {
                comparison = leftParts[index].compareToIgnoreCase(rightParts[index]);
            }
            if (comparison != 0) {
                return comparison;
            }
        }
        return Integer.compare(leftParts.length, rightParts.length);
    }
}
