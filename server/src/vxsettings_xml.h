#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {

// Small self-contained XML element reader for vxsettings files. Handles a
// UTF-8 BOM, the `<?xml ?>` prolog, `<!-- -->` comments, `<!DOCTYPE>`, CDATA,
// self-closing tags, attributes in either quote, and the five named entities
// plus numeric character references. No namespaces, no DTD expansion. Text is
// the concatenated character data directly inside the element (UTF-8, entities
// decoded, not trimmed). The macro library reader (macro_library_xml.h) keeps
// its own scanner.

struct XmlElement
{
  std::string name;
  std::vector<std::pair<std::string, std::string>> attributes;
  std::vector<XmlElement> children;
  std::string text;

  // Exact-name lookups; the first match wins.
  [[nodiscard]] auto attribute(std::string_view key) const -> std::optional<std::string_view>;
  [[nodiscard]] auto child(std::string_view childName) const -> const XmlElement*;
};

// The root element, or nullopt on malformed input (unclosed or mismatched
// tags, a second root, nothing at all).
[[nodiscard]] auto parseXml(std::string_view text) -> std::optional<XmlElement>;

} // namespace vertexadt
