#include "vxsettings_xml.h"

#include <cstdint>

namespace vertexadt {
namespace {

bool isXmlSpace(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool isNameChar(char ch)
{
  return !isXmlSpace(ch) && ch != '>' && ch != '/' && ch != '=' && ch != '<' && ch != '"'
    && ch != '\'';
}

void appendUtf8(std::string& out, std::uint32_t cp)
{
  if (cp < 0x80)
  {
    out += static_cast<char>(cp);
  }
  else if (cp < 0x800)
  {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
  else if (cp < 0x10000)
  {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
  else if (cp < 0x110000)
  {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// Appends `raw` with entities decoded. An unknown or malformed reference is
// kept as written.
void appendDecoded(std::string& out, std::string_view raw)
{
  std::size_t i = 0;
  while (i < raw.size())
  {
    const auto amp = raw.find('&', i);
    if (amp == std::string_view::npos)
    {
      out.append(raw.substr(i));
      return;
    }
    out.append(raw.substr(i, amp - i));
    const auto semi = raw.find(';', amp);
    if (semi == std::string_view::npos || semi - amp > 12)
    {
      out += '&';
      i = amp + 1;
      continue;
    }
    const auto name = raw.substr(amp + 1, semi - amp - 1);
    bool known = true;
    if (name == "lt")
    {
      out += '<';
    }
    else if (name == "gt")
    {
      out += '>';
    }
    else if (name == "amp")
    {
      out += '&';
    }
    else if (name == "quot")
    {
      out += '"';
    }
    else if (name == "apos")
    {
      out += '\'';
    }
    else if (name.size() >= 2 && name[0] == '#')
    {
      const bool hex = name[1] == 'x' || name[1] == 'X';
      const auto digits = name.substr(hex ? 2 : 1);
      std::uint32_t cp = 0;
      known = !digits.empty();
      for (const auto ch : digits)
      {
        int value = -1;
        if (ch >= '0' && ch <= '9')
        {
          value = ch - '0';
        }
        else if (hex && ch >= 'a' && ch <= 'f')
        {
          value = ch - 'a' + 10;
        }
        else if (hex && ch >= 'A' && ch <= 'F')
        {
          value = ch - 'A' + 10;
        }
        if (value < 0 || cp > 0x10FFFF)
        {
          known = false;
          break;
        }
        cp = cp * (hex ? 16 : 10) + static_cast<std::uint32_t>(value);
      }
      if (known)
      {
        appendUtf8(out, cp);
      }
    }
    else
    {
      known = false;
    }
    if (!known)
    {
      out.append(raw.substr(amp, semi - amp + 1));
    }
    i = semi + 1;
  }
}

class Parser
{
public:
  explicit Parser(std::string_view text) : text_(text) {}

  auto run() -> std::optional<XmlElement>
  {
    if (text_.starts_with("\xEF\xBB\xBF"))
    {
      pos_ = 3;
    }
    std::optional<XmlElement> root;
    while (true)
    {
      skipSpace();
      if (pos_ >= text_.size())
      {
        break;
      }
      if (text_[pos_] != '<')
      {
        return std::nullopt;
      }
      if (skipMarkup())
      {
        if (failed_)
        {
          return std::nullopt;
        }
        continue;
      }
      if (root)
      {
        return std::nullopt;
      }
      auto element = parseElement();
      if (!element)
      {
        return std::nullopt;
      }
      root = std::move(element);
    }
    return root;
  }

private:
  std::string_view text_;
  std::size_t pos_ = 0;
  bool failed_ = false;
  int depth_ = 0;

  void skipSpace()
  {
    while (pos_ < text_.size() && isXmlSpace(text_[pos_]))
    {
      ++pos_;
    }
  }

  // At `<`: skips a comment, processing instruction or declaration. Returns
  // false (position unchanged) for anything else. Unterminated markup fails.
  bool skipMarkup()
  {
    const auto rest = text_.substr(pos_);
    std::string_view close;
    if (rest.starts_with("<!--"))
    {
      close = "-->";
    }
    else if (rest.starts_with("<?"))
    {
      close = "?>";
    }
    else if (rest.starts_with("<!") && !rest.starts_with("<![CDATA["))
    {
      close = ">";
    }
    else
    {
      return false;
    }
    const auto end = text_.find(close, pos_ + 2);
    if (end == std::string_view::npos)
    {
      failed_ = true;
      pos_ = text_.size();
      return true;
    }
    pos_ = end + close.size();
    return true;
  }

  auto parseName() -> std::string
  {
    const auto start = pos_;
    while (pos_ < text_.size() && isNameChar(text_[pos_]))
    {
      ++pos_;
    }
    return std::string(text_.substr(start, pos_ - start));
  }

  // At `<name`.
  auto parseElement() -> std::optional<XmlElement>
  {
    if (++depth_ > 256)
    {
      return std::nullopt;
    }
    ++pos_;
    XmlElement element;
    element.name = parseName();
    if (element.name.empty())
    {
      return std::nullopt;
    }
    while (true)
    {
      skipSpace();
      if (pos_ >= text_.size())
      {
        return std::nullopt;
      }
      if (text_.substr(pos_).starts_with("/>"))
      {
        pos_ += 2;
        --depth_;
        return element;
      }
      if (text_[pos_] == '>')
      {
        ++pos_;
        break;
      }
      auto key = parseName();
      if (key.empty())
      {
        return std::nullopt;
      }
      skipSpace();
      if (pos_ >= text_.size() || text_[pos_] != '=')
      {
        return std::nullopt;
      }
      ++pos_;
      skipSpace();
      if (pos_ >= text_.size() || (text_[pos_] != '"' && text_[pos_] != '\''))
      {
        return std::nullopt;
      }
      const auto quote = text_[pos_++];
      const auto end = text_.find(quote, pos_);
      if (end == std::string_view::npos)
      {
        return std::nullopt;
      }
      std::string value;
      appendDecoded(value, text_.substr(pos_, end - pos_));
      pos_ = end + 1;
      element.attributes.emplace_back(std::move(key), std::move(value));
    }
    // Content.
    while (true)
    {
      if (pos_ >= text_.size())
      {
        return std::nullopt;
      }
      if (text_[pos_] != '<')
      {
        const auto next = text_.find('<', pos_);
        const auto end = next == std::string_view::npos ? text_.size() : next;
        appendDecoded(element.text, text_.substr(pos_, end - pos_));
        pos_ = end;
        continue;
      }
      const auto rest = text_.substr(pos_);
      if (rest.starts_with("<![CDATA["))
      {
        const auto end = text_.find("]]>", pos_ + 9);
        if (end == std::string_view::npos)
        {
          return std::nullopt;
        }
        element.text.append(text_.substr(pos_ + 9, end - pos_ - 9));
        pos_ = end + 3;
        continue;
      }
      if (rest.starts_with("</"))
      {
        pos_ += 2;
        const auto closing = parseName();
        skipSpace();
        if (closing != element.name || pos_ >= text_.size() || text_[pos_] != '>')
        {
          return std::nullopt;
        }
        ++pos_;
        --depth_;
        return element;
      }
      if (skipMarkup())
      {
        if (failed_)
        {
          return std::nullopt;
        }
        continue;
      }
      auto child = parseElement();
      if (!child)
      {
        return std::nullopt;
      }
      element.children.push_back(std::move(*child));
    }
  }
};

} // namespace

auto XmlElement::attribute(std::string_view key) const -> std::optional<std::string_view>
{
  for (const auto& [name, value] : attributes)
  {
    if (name == key)
    {
      return std::string_view(value);
    }
  }
  return std::nullopt;
}

auto XmlElement::child(std::string_view childName) const -> const XmlElement*
{
  for (const auto& element : children)
  {
    if (element.name == childName)
    {
      return &element;
    }
  }
  return nullptr;
}

auto parseXml(std::string_view text) -> std::optional<XmlElement>
{
  return Parser(text).run();
}

} // namespace vertexadt
