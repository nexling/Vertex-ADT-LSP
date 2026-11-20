#include "validate.h"
#include "utf16_text.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vertexadt {
namespace {

struct JointSectionRule
{
  bool allowMacro = true;
  bool allowJoint = false;
  bool requireExitBeforeMacro = false;
};

bool isAsciiWordChar(unsigned char ch)
{
  return (ch >= 'A' && ch <= 'Z')
    || (ch >= 'a' && ch <= 'z')
    || (ch >= '0' && ch <= '9')
    || ch == '_';
}

bool isAsciiSpace(unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

char asciiLower(unsigned char ch)
{
  if (ch >= 'A' && ch <= 'Z')
  {
    return static_cast<char>(ch - 'A' + 'a');
  }
  return static_cast<char>(ch);
}

bool wordAt(std::string_view text, std::size_t offset, std::string_view word)
{
  if (offset + word.size() > text.size())
  {
    return false;
  }
  if (offset > 0 && isAsciiWordChar(static_cast<unsigned char>(text[offset - 1])))
  {
    return false;
  }
  for (std::size_t i = 0; i < word.size(); ++i)
  {
    if (asciiLower(static_cast<unsigned char>(text[offset + i]))
        != asciiLower(static_cast<unsigned char>(word[i])))
    {
      return false;
    }
  }
  const auto after = offset + word.size();
  return after == text.size() || !isAsciiWordChar(static_cast<unsigned char>(text[after]));
}

bool lineHasCommand(std::string_view codeLine, std::string_view command)
{
  if (command.empty() || command.size() > codeLine.size())
  {
    return false;
  }
  for (std::size_t i = 0; i + command.size() <= codeLine.size(); ++i)
  {
    if (wordAt(codeLine, i, command))
    {
      return true;
    }
  }
  return false;
}

bool hasThenExit(std::string_view code)
{
  constexpr std::string_view thenWord = "then";
  constexpr std::string_view exitWord = "exit";
  if (code.size() < thenWord.size() + 1 + exitWord.size())
  {
    return false;
  }
  for (std::size_t i = 0; i + thenWord.size() <= code.size(); ++i)
  {
    if (!wordAt(code, i, thenWord))
    {
      continue;
    }
    auto j = i + thenWord.size();
    if (j >= code.size() || !isAsciiSpace(static_cast<unsigned char>(code[j])))
    {
      continue;
    }
    while (j < code.size() && isAsciiSpace(static_cast<unsigned char>(code[j])))
    {
      ++j;
    }
    if (wordAt(code, j, exitWord))
    {
      return true;
    }
  }
  return false;
}

auto getCodeLine(std::string_view line) -> std::string
{
  if (isBangComment(line))
  {
    return {};
  }
  return std::string(trim(stripValidationComment(line)));
}

auto rulesFor(std::string_view name) -> const JointSectionRule*
{
  static const JointSectionRule updateJoint{true, false, true};
  static const JointSectionRule jointTest{false, false, false};
  static const JointSectionRule editProperties{true, false, false};
  if (name == "UPDATE_JOINT" || name == "JOINT_DETAILING" || name == "JOINT_ADAPT")
  {
    return &updateJoint;
  }
  if (name == "JOINT_TEST")
  {
    return &jointTest;
  }
  if (name == "EDIT_PROPERTIES")
  {
    return &editProperties;
  }
  return nullptr;
}

auto lineTextForEnvelope(std::string_view raw) -> std::string
{
  if (!raw.empty() && raw.back() == '\r')
  {
    raw.remove_suffix(1);
  }
  return std::string(raw);
}

bool hasIfThenExitFailsafe(
  const std::vector<std::string_view>& lines,
  int fromLine,
  int beforeLine)
{
  for (int i = fromLine; i < beforeLine; ++i)
  {
    const auto code = getCodeLine(lines[static_cast<std::size_t>(i)]);
    if (code.empty())
    {
      continue;
    }
    if (lineHasCommand(code, "if") && lineHasCommand(code, "then") && !hasThenExit(code))
    {
      bool hasReturn = false;
      for (int j = i + 1; j < beforeLine; ++j)
      {
        const auto inner = getCodeLine(lines[static_cast<std::size_t>(j)]);
        if (inner.empty())
        {
          continue;
        }
        if (wordAt(inner, 0, "endif"))
        {
          break;
        }
        if (inner.find("set") != std::string::npos
            && inner.find("RETURN") != std::string::npos
            && (inner.find("\"1\"") != std::string::npos || inner.find("\"0\"") != std::string::npos))
        {
          hasReturn = true;
        }
        if (hasReturn && wordAt(inner, 0, "exit"))
        {
          return true;
        }
      }
    }
  }
  return false;
}

void emitWrappedWarning(ValidateContext& ctx, int line, std::string inner)
{
  const auto lineText = lineTextForEnvelope(ctx.prep->lines[static_cast<std::size_t>(line)]);
  const auto range = lsp::Range{
    .start = {.line = static_cast<lsp::Uint>(line), .character = 0},
    .end = {
      .line = static_cast<lsp::Uint>(line),
      .character = static_cast<lsp::Uint>(utf16Length(lineText)),
    },
  };
  lsp::Diagnostic diagnostic;
  diagnostic.range = range;
  diagnostic.message = lineText + " has an error: " + inner;
  diagnostic.severity = lsp::DiagnosticSeverity::Warning;
  diagnostic.source = "ADT-Syntax";
  diagnostic.relatedInformation = std::vector<lsp::DiagnosticRelatedInformation>{
    lsp::DiagnosticRelatedInformation{
      .location = {.uri = ctx.prep->snapshot.uri, .range = range},
      .message = inner,
    },
  };
  ctx.append(std::move(diagnostic));
}

void checkJointSection(ValidateContext& ctx, const NamedSection& section)
{
  if (isVerificationIgnoredSection(section.name))
  {
    return;
  }
  const auto* rules = rulesFor(section.name);
  if (rules == nullptr)
  {
    return;
  }

  const int bodyStart = section.openLine + 1;
  const int bodyEnd = section.closeLine - 1;
  for (int i = bodyStart; i <= bodyEnd; ++i)
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      return;
    }

    const auto code = getCodeLine(ctx.prep->lines[static_cast<std::size_t>(i)]);
    if (code.empty())
    {
      continue;
    }

    if (!rules->allowMacro && lineHasCommand(code, "new_macro"))
    {
      emitWrappedWarning(ctx, i, "new_macro is not allowed in " + section.name + ".");
      continue;
    }

    if (!rules->allowJoint && lineHasCommand(code, "new_djoint"))
    {
      emitWrappedWarning(ctx, i, "new_djoint is not allowed in " + section.name + ".");
      continue;
    }

    if (rules->requireExitBeforeMacro && lineHasCommand(code, "new_macro"))
    {
      if (!hasIfThenExitFailsafe(ctx.prep->lines, bodyStart, i))
      {
        emitWrappedWarning(
          ctx,
          i,
          "new_macro in " + section.name
            + " requires an if-then-exit failsafe earlier in this section.");
      }
    }
  }
}

} // namespace

void familyJointSection(ValidateContext& ctx)
{
  for (const auto& section : ctx.prep->sections)
  {
    ctx.pollCancel();
    if (ctx.atCap())
    {
      break;
    }
    checkJointSection(ctx, section);
  }
}

} // namespace vertexadt
