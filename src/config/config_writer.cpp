#include "config/config_writer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace config {
    namespace {

        constexpr size_t CONFIG_WRITER_MAX_FILE_SIZE = 16 * 1024 * 1024;
        constexpr size_t CONFIG_WRITER_MAX_INCLUDE_DEPTH = 64;

        bool Fail(s_cfg_diagnostic* diagnostic, const s_source_location& location,
            const std::string& message) {
            diagnostic->location = location;
            diagnostic->message = message;
            return false;
        }

        s_source_location Location_At(const char* source_name, size_t offset, uint32_t line) {
            s_source_location location;
            location.source = source_name == nullptr ? "" : source_name;
            location.offset = offset;
            location.line = line;
            location.column = 1;
            return location;
        }

        bool Is_Identifier_Start(char character) {
            return (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') || character == '_';
        }

        bool Is_Identifier_Continue(char character) {
            return Is_Identifier_Start(character) ||
                (character >= '0' && character <= '9') || character == '-' || character == '.';
        }

        // Same identifier rule as the parser: a bare token that is not a
        // number, ratio or mask. Mod IDs such as `v150i`, `ice-x` and
        // `example-balance.mod` all satisfy it.
        bool Is_Valid_Mod_Id(const std::string& value) {
            if (value.empty() || !Is_Identifier_Start(value.front())) {
                return false;
            }
            for (char character : value) {
                if (!Is_Identifier_Continue(character)) {
                    return false;
                }
            }
            return true;
        }

        struct s_enable_span {
            size_t start = 0; // Offset of the `enable` keyword.
            size_t end = 0; // Offset just past the terminating `;`.
            uint32_t line = 1;
            std::string id;
        };

        struct s_edit {
            size_t start = 0;
            size_t end = 0;
            std::string text;
        };

        bool Is_Space_Or_Tab(char character) {
            return character == ' ' || character == '\t';
        }

        // The byte range of one enable statement. The statement must be a
        // single-line `enable <id>;` with only spaces or tabs between its
        // parts: a comment or line break inside it would be dropped by a
        // surgical replacement, so that form is refused instead.
        bool Find_Enable_Span(const std::string& source, const s_cfg_statement& statement,
            s_enable_span* span, s_cfg_diagnostic* diagnostic) {
            const s_cfg_token& argument = statement.argument;
            size_t offset = statement.location.offset;
            if (source.compare(offset, 6, "enable") != 0) {
                return Fail(diagnostic, statement.location,
                    "enable statement at line " + std::to_string(statement.location.line) +
                        " has a form the writer does not understand");
            }
            offset += 6;
            size_t gap = 0;
            while (offset < source.size() && Is_Space_Or_Tab(source[offset])) {
                ++offset;
                ++gap;
            }
            if (gap == 0 || source.compare(offset, argument.raw.size(), argument.raw) != 0) {
                return Fail(diagnostic, statement.location,
                    "enable statement for mod '" + argument.value + "' at line " +
                        std::to_string(statement.location.line) +
                        " has a form the writer does not understand");
            }
            offset += argument.raw.size();
            while (offset < source.size() && Is_Space_Or_Tab(source[offset])) {
                ++offset;
            }
            if (offset >= source.size() || source[offset] != ';') {
                return Fail(diagnostic, statement.location,
                    "enable statement for mod '" + argument.value + "' at line " +
                        std::to_string(statement.location.line) +
                        " is not a single-line 'enable <id>;' statement; refusing to edit it");
            }
            ++offset;
            span->start = statement.location.offset;
            span->end = offset;
            span->line = statement.location.line;
            span->id = argument.value;
            return true;
        }

        // Byte range to delete for a dropped enable statement: the whole
        // line (including its newline) when every non-whitespace byte on it
        // belongs to a removed enable statement, so no blank or
        // whitespace-only line is left behind; just the statement itself
        // when it shares the line with surviving content, so that content
        // survives. Several removed statements on one line share one
        // whole-line range; callers emit it once.
        std::pair<size_t, size_t> Removal_Range(const std::string& source,
            const s_enable_span& span, const std::vector<s_enable_span>& removed_spans) {
            const size_t line_start = source.rfind('\n', span.start == 0 ? 0 : span.start - 1);
            const size_t line_begin = line_start == std::string::npos ? 0 : line_start + 1;
            const size_t line_end = source.find('\n', span.end);
            const size_t line_tail = line_end == std::string::npos ? source.size() : line_end;
            bool only_removed = true;
            for (size_t offset = line_begin; offset < line_tail; ++offset) {
                const char character = source[offset];
                if (character == ' ' || character == '\t' || character == '\r') {
                    continue;
                }
                bool in_removed_span = false;
                for (const s_enable_span& removed : removed_spans) {
                    if (offset >= removed.start && offset < removed.end) {
                        in_removed_span = true;
                        break;
                    }
                }
                if (!in_removed_span) {
                    only_removed = false;
                    break;
                }
            }
            if (only_removed) {
                const size_t remove_end =
                    line_end == std::string::npos ? source.size() : line_end + 1;
                return {line_begin, remove_end};
            }
            return {span.start, span.end};
        }

        // Applies surgical edits (sorted, non-overlapping byte ranges) to
        // a source document. Shared by the enable and settings rewriters.
        std::string Apply_Edits(const std::string& source,
            const std::vector<s_edit>& edits) {
            std::string result;
            size_t cursor = 0;
            for (const s_edit& edit : edits) {
                result.append(source, cursor, edit.start - cursor);
                result.append(edit.text);
                cursor = edit.end;
            }
            result.append(source, cursor, std::string::npos);
            return result;
        }

        // The file's line-ending convention, taken from its first line;
        // appended lines match it. Shared by both rewriters.
        std::string File_Newline(const std::string& source) {
            const size_t first_newline = source.find('\n');
            return first_newline != std::string::npos && first_newline > 0 &&
                    source[first_newline - 1] == '\r'
                ? "\r\n"
                : "\n";
        }

        bool Is_Decimal_Index(const std::string& value) {
            if (value.empty()) {
                return false;
            }
            for (char character : value) {
                if (character < '0' || character > '9') {
                    return false;
                }
            }
            return true;
        }

        // A setting-path segment is an identifier (a name) or a decimal
        // array index, mirroring the parser's path-token rule.
        bool Is_Valid_Path_Segment(const std::string& value) {
            if (value.empty()) {
                return false;
            }
            if (Is_Identifier_Start(value.front())) {
                for (char character : value) {
                    if (!Is_Identifier_Continue(character)) {
                        return false;
                    }
                }
                return true;
            }
            return Is_Decimal_Index(value);
        }

        // The value must parse as exactly one value token whose raw text
        // is the value itself: anything else (a second token, an
        // unterminated string, a bad escape, a stray ';') would corrupt
        // the rewritten file. The skip marker "-" is refused outright:
        // writing it would not set the value.
        bool Validate_Setting_Value(const char* source_name, const std::string& value,
            s_cfg_diagnostic* diagnostic) {
            if (value.empty()) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting value is empty; refusing to write it");
            }
            if (value.find('\n') != std::string::npos ||
                value.find('\r') != std::string::npos) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting value contains a line break; refusing to write it");
            }
            if (value == "-") {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting value '-' is the skip marker, not a value; refusing to write it");
            }
            const std::string synthetic = "__setting__ = " + value + ";";
            s_cfg_document document;
            s_cfg_diagnostic parse_diagnostic;
            if (!Parse_Cfg(source_name, synthetic.data(), synthetic.size(), &document,
                    &parse_diagnostic)) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting value '" + value + "' does not parse as a single value (" +
                        parse_diagnostic.message + "); refusing to write it");
            }
            if (document.statements.size() != 1 ||
                document.statements[0].kind != CFG_STATEMENT_ASSIGNMENT ||
                document.statements[0].values.size() != 1 ||
                !document.statements[0].groups.empty() ||
                document.statements[0].values[0].raw != value) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting value '" + value +
                        "' does not parse as a single value; refusing to write it");
            }
            return true;
        }

        struct s_setting_span {
            size_t start = 0; // Offset of the first path token.
            size_t value_start = 0; // Offset of the value token.
            size_t value_end = 0; // Offset just past the value token.
            size_t end = 0; // Offset just past the terminating `;`.
            uint32_t line = 1;
        };

        // The byte ranges of one setting assignment. The statement must
        // be a plain single-line `<path> = <value>;` assignment with
        // exactly one value and no labeled groups: a comment or line
        // break inside it would be dropped by a surgical replacement, so
        // that form is refused instead. The scan tracks string literals,
        // so a '#' or ';' inside a quoted value is not mistaken for a
        // comment or the terminator.
        bool Find_Setting_Span(const std::string& source,
            const s_cfg_statement& statement, s_setting_span* span,
            s_cfg_diagnostic* diagnostic) {
            if (statement.values.size() != 1 || !statement.groups.empty()) {
                return Fail(diagnostic, statement.location,
                    "assignment at line " + std::to_string(statement.location.line) +
                        " is not a plain single-value assignment; refusing to edit it");
            }
            const s_cfg_token& value = statement.values[0];
            size_t offset = statement.location.offset;
            bool in_string = false;
            size_t semicolon = std::string::npos;
            while (offset < source.size()) {
                const char character = source[offset];
                if (in_string) {
                    if (character == '\\') {
                        offset += 2;
                        continue;
                    }
                    if (character == '"') {
                        in_string = false;
                    } else if (character == '\n' || character == '\r') {
                        return Fail(diagnostic, statement.location,
                            "assignment at line " +
                                std::to_string(statement.location.line) +
                                " is not a single-line assignment; refusing to edit it");
                    }
                    ++offset;
                    continue;
                }
                if (character == '"') {
                    in_string = true;
                    ++offset;
                    continue;
                }
                if (character == '#') {
                    return Fail(diagnostic, statement.location,
                        "assignment at line " + std::to_string(statement.location.line) +
                            " has a comment inside it; refusing to edit it");
                }
                if (character == '\n' || character == '\r') {
                    return Fail(diagnostic, statement.location,
                        "assignment at line " + std::to_string(statement.location.line) +
                            " is not a single-line assignment; refusing to edit it");
                }
                if (character == ';') {
                    semicolon = offset;
                    break;
                }
                ++offset;
            }
            if (semicolon == std::string::npos) {
                return Fail(diagnostic, statement.location,
                    "assignment at line " + std::to_string(statement.location.line) +
                        " has a form the writer does not understand");
            }
            const size_t value_start = value.location.offset;
            const size_t value_end = value_start + value.raw.size();
            if (value_start < statement.location.offset || value_end > semicolon) {
                return Fail(diagnostic, statement.location,
                    "assignment at line " + std::to_string(statement.location.line) +
                        " has a form the writer does not understand");
            }
            for (size_t index = value_end; index < semicolon; ++index) {
                if (source[index] != ' ' && source[index] != '\t') {
                    return Fail(diagnostic, statement.location,
                        "assignment at line " + std::to_string(statement.location.line) +
                            " has a form the writer does not understand");
                }
            }
            span->start = statement.location.offset;
            span->value_start = value_start;
            span->value_end = value_end;
            span->end = semicolon + 1;
            span->line = statement.location.line;
            return true;
        }

        // Same path-equality rule the loader applies to statement paths:
        // identifiers compare by value, decimal indexes by their digits.
        bool Paths_Equal(const std::vector<s_cfg_token>& path,
            const std::vector<std::string>& wanted) {
            if (path.size() != wanted.size()) {
                return false;
            }
            for (size_t index = 0; index < path.size(); ++index) {
                if (path[index].value != wanted[index]) {
                    return false;
                }
            }
            return true;
        }

        std::string Join_Setting_Path(const std::vector<std::string>& setting_path) {
            std::string joined;
            for (size_t index = 0; index < setting_path.size(); ++index) {
                if (index != 0) {
                    joined += ' ';
                }
                joined += setting_path[index];
            }
            return joined;
        }

        // `setting_path` selects the include check: null means the enable
        // writer (refuse when an include enables a mod); non-null means
        // the settings writer (refuse when an include assigns that path).
        bool Check_Includes(const std::filesystem::path& config_directory,
            const s_cfg_document& document, const char* source_name,
            std::unordered_set<std::string>* seen, size_t depth,
            const std::vector<std::string>* setting_path,
            s_cfg_diagnostic* diagnostic) {
            for (const s_cfg_statement& statement : document.statements) {
                if (statement.kind == CFG_STATEMENT_STOP) {
                    break;
                }
                if (statement.kind != CFG_STATEMENT_INCLUDE) {
                    continue;
                }
                const std::string& target = statement.argument.value;
                std::filesystem::path resolved(target);
                if (resolved.is_relative()) {
                    resolved = config_directory / resolved;
                }
                std::error_code error;
                if (!std::filesystem::is_regular_file(resolved, error)) {
                    return Fail(diagnostic, statement.argument.location,
                        "include '" + target + "' at line " +
                            std::to_string(statement.argument.location.line) +
                            " cannot be read ('" + resolved.string() +
                            "'); refusing to edit a config whose includes cannot be resolved");
                }
                const std::filesystem::path canonical =
                    std::filesystem::canonical(resolved, error);
                if (error) {
                    return Fail(diagnostic, statement.argument.location,
                        "include '" + target + "' at line " +
                            std::to_string(statement.argument.location.line) +
                            " cannot be resolved ('" + resolved.string() + "')");
                }
                const std::string key = canonical.string();
                if (seen->find(key) != seen->end()) {
                    return Fail(diagnostic, statement.argument.location,
                        "include cycle at '" + target + "' at line " +
                            std::to_string(statement.argument.location.line));
                }
                // Mirror the loader: it fails on entry when the include
                // stack reaches 64 files, so at most 63 nested includes
                // (64 files including the entry file) are tolerated. An
                // include in a document at depth `depth` is the
                // (depth + 1)-th nested include.
                if (depth + 1 >= CONFIG_WRITER_MAX_INCLUDE_DEPTH) {
                    return Fail(diagnostic, statement.argument.location,
                        "include depth exceeds 64 files at '" + target + "' at line " +
                            std::to_string(statement.argument.location.line));
                }
                std::ifstream input(canonical, std::ios::binary);
                if (!input) {
                    return Fail(diagnostic, statement.argument.location,
                        "include '" + target + "' at line " +
                            std::to_string(statement.argument.location.line) +
                            " cannot be read ('" + canonical.string() + "')");
                }
                const std::string included(
                    (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
                if (input.bad()) {
                    return Fail(diagnostic, statement.argument.location,
                        "include '" + target + "' at line " +
                            std::to_string(statement.argument.location.line) +
                            " cannot be read ('" + canonical.string() + "')");
                }
                s_cfg_document included_document;
                s_cfg_diagnostic included_diagnostic;
                if (!Parse_Cfg(canonical.string().c_str(), included.data(), included.size(),
                        &included_document, &included_diagnostic)) {
                    *diagnostic = included_diagnostic;
                    return false;
                }
                for (const s_cfg_statement& included_statement : included_document.statements) {
                    if (included_statement.kind == CFG_STATEMENT_STOP) {
                        break;
                    }
                    if (setting_path == nullptr) {
                        if (included_statement.kind == CFG_STATEMENT_ENABLE) {
                            return Fail(diagnostic, included_statement.location,
                                "include '" + canonical.string() + "' enables mod '" +
                                    included_statement.argument.value + "' at line " +
                                    std::to_string(included_statement.location.line) +
                                    "; the writer only updates enable lines in '" + source_name +
                                    "', so the written set would not match the effective set");
                        }
                    } else if (included_statement.kind == CFG_STATEMENT_ASSIGNMENT &&
                        Paths_Equal(included_statement.path, *setting_path)) {
                        return Fail(diagnostic, included_statement.location,
                            "include '" + canonical.string() + "' assigns '" +
                                Join_Setting_Path(*setting_path) + "' at line " +
                                std::to_string(included_statement.location.line) +
                                "; the writer only updates assignments in '" + source_name +
                                "', so the written value would not match the effective value");
                    }
                }
                seen->insert(key);
                // Nested includes resolve lexically beside the including
                // file as referenced, mirroring the loader. The canonical
                // path is only for cycle detection and reading.
                const std::filesystem::path included_directory = resolved.parent_path();
                if (!Check_Includes(included_directory, included_document, source_name, seen,
                        depth + 1, setting_path, diagnostic)) {
                    return false;
                }
                seen->erase(key);
            }
            return true;
        }

    } // namespace

    namespace {

        // Reads a config file for a Write_ call: the file must exist and
        // parse, and `target` is its canonical path, so a symlinked config
        // is written through and the link itself is preserved.
        bool Read_Config_File(const std::string& path, std::string* source,
            std::filesystem::path* target, s_cfg_document* document,
            s_cfg_diagnostic* diagnostic) {
            const s_source_location file_location = Location_At(path.c_str(), 0, 1);
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) {
                return Fail(diagnostic, file_location,
                    "cannot read configuration file '" + path + "'");
            }
            const uintmax_t size = std::filesystem::file_size(path, error);
            if (error || size > CONFIG_WRITER_MAX_FILE_SIZE) {
                return Fail(diagnostic, file_location,
                    "configuration file '" + path + "' is too large to edit safely");
            }
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                return Fail(diagnostic, file_location,
                    "cannot read configuration file '" + path + "'");
            }
            *source = std::string(
                (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            if (input.bad()) {
                return Fail(diagnostic, file_location,
                    "cannot read configuration file '" + path + "'");
            }

            // Write through a symlink: resolve the canonical target and
            // edit that file, so the link itself (and anything else
            // pointing at the target) keeps working. A rename over the
            // link would replace it with a regular file and leave the
            // target stale.
            error.clear();
            *target = std::filesystem::canonical(path, error);
            if (error) {
                return Fail(diagnostic, file_location,
                    "cannot resolve configuration file '" + path + "'");
            }
            if (!Parse_Cfg(path.c_str(), source->data(), source->size(), document,
                    diagnostic)) {
                return false;
            }
            return true;
        }

        // Replaces a config file atomically: the new content goes to a
        // temporary file in the same directory, takes the original's
        // permissions, and is renamed over it. Any failure leaves the
        // original untouched. `path` is only used in diagnostics.
        bool Write_Rewritten_File(const std::string& path,
            const std::filesystem::path& target, const std::string& rewritten,
            s_cfg_diagnostic* diagnostic) {
            const s_source_location file_location = Location_At(path.c_str(), 0, 1);
            std::string suffix;
            {
                std::random_device device;
                std::ostringstream stream;
                stream << std::hex << device() << device();
                suffix = stream.str();
            }
            const std::filesystem::path temporary = target.string() + ".tmp-" + suffix;
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) {
                    return Fail(diagnostic, file_location,
                        "cannot write temporary file '" + temporary.string() + "'");
                }
                output.write(rewritten.data(), static_cast<std::streamsize>(rewritten.size()));
                output.flush();
                if (!output) {
                    std::error_code remove_error;
                    std::filesystem::remove(temporary, remove_error);
                    return Fail(diagnostic, file_location,
                        "cannot write temporary file '" + temporary.string() + "'");
                }
                output.close();
                if (!output) {
                    std::error_code remove_error;
                    std::filesystem::remove(temporary, remove_error);
                    return Fail(diagnostic, file_location,
                        "cannot write temporary file '" + temporary.string() + "'");
                }
            }
            std::error_code status_error;
            const std::filesystem::perms permissions =
                std::filesystem::status(target, status_error).permissions();
            if (status_error) {
                std::error_code remove_error;
                std::filesystem::remove(temporary, remove_error);
                return Fail(diagnostic, file_location,
                    "cannot read permissions of configuration file '" + path + "'");
            }
            std::error_code permissions_error;
            std::filesystem::permissions(temporary, permissions, permissions_error);
            if (permissions_error) {
                std::error_code remove_error;
                std::filesystem::remove(temporary, remove_error);
                return Fail(diagnostic, file_location,
                    "cannot copy permissions to temporary file '" + temporary.string() + "'");
            }
            std::error_code error;
            std::filesystem::rename(temporary, target, error);
            if (error) {
                std::error_code remove_error;
                std::filesystem::remove(temporary, remove_error);
                return Fail(diagnostic, file_location,
                    "cannot replace configuration file '" + path + "'");
            }
            return true;
        }

        // The include check both Write_ calls share, with the setting
        // path selecting the mode (null for the enable writer). Includes
        // resolve lexically beside the path as passed, mirroring the
        // loader (which never resolves symlinks when locating includes).
        // Only the write itself goes through the canonical target;
        // resolving includes beside the target would check the wrong
        // directory when the config is a symlink into another directory.
        bool Check_Write_Includes(const std::string& path,
            const std::filesystem::path& target, const s_cfg_document& document,
            const std::vector<std::string>* setting_path,
            s_cfg_diagnostic* diagnostic) {
            std::unordered_set<std::string> seen;
            seen.insert(target.string());
            const std::filesystem::path lexical_directory =
                std::filesystem::absolute(path).parent_path();
            return Check_Includes(lexical_directory, document, path.c_str(),
                &seen, 0, setting_path, diagnostic);
        }

    } // namespace

    bool Rewrite_Enabled_Mods(const char* source_name, const std::string& source,
        const std::vector<std::string>& enabled_mods, std::string* rewritten,
        s_cfg_diagnostic* diagnostic) {
        if (source_name == nullptr || rewritten == nullptr || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        for (const std::string& id : enabled_mods) {
            if (!Is_Valid_Mod_Id(id)) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "mod ID '" + id + "' is not a valid identifier; refusing to write it");
            }
        }
        for (size_t first = 0; first < enabled_mods.size(); ++first) {
            for (size_t second = first + 1; second < enabled_mods.size(); ++second) {
                if (enabled_mods[first] == enabled_mods[second]) {
                    return Fail(diagnostic, Location_At(source_name, 0, 1),
                        "duplicate mod ID '" + enabled_mods[first] +
                            "' in the desired enabled set; refusing to write it");
                }
            }
        }

        s_cfg_document document;
        if (!Parse_Cfg(source_name, source.data(), source.size(), &document, diagnostic)) {
            return false;
        }

        std::vector<s_enable_span> spans;
        bool has_stop = false;
        s_source_location stop_location;
        for (const s_cfg_statement& statement : document.statements) {
            if (statement.kind == CFG_STATEMENT_STOP) {
                has_stop = true;
                stop_location = statement.location;
                break;
            }
            if (statement.kind != CFG_STATEMENT_ENABLE) {
                continue;
            }
            spans.emplace_back();
            if (!Find_Enable_Span(source, statement, &spans.back(), diagnostic)) {
                return false;
            }
        }

        if (spans.empty() && has_stop && !enabled_mods.empty()) {
            return Fail(diagnostic, stop_location,
                "config has a 'stop' statement at line " +
                    std::to_string(stop_location.line) +
                    " and no enable lines; new enable lines appended at the end of the "
                    "file would sit after 'stop' and never take effect; refusing to edit it");
        }

        // The first line's ending defines the file's line-ending
        // convention; appended lines match it.
        const std::string newline = File_Newline(source);
        const size_t existing_count = spans.size();
        const size_t desired_count = enabled_mods.size();
        const size_t shared = existing_count < desired_count ? existing_count : desired_count;
        std::vector<s_edit> edits;

        for (size_t index = 0; index < shared; ++index) {
            if (enabled_mods[index] == spans[index].id) {
                continue;
            }
            edits.push_back(
                {spans[index].start, spans[index].end, "enable " + enabled_mods[index] + ";"});
        }
        std::vector<s_enable_span> removed_spans;
        for (size_t index = desired_count; index < existing_count; ++index) {
            removed_spans.push_back(spans[index]);
        }
        std::vector<std::pair<size_t, size_t>> removal_ranges;
        for (size_t index = desired_count; index < existing_count; ++index) {
            const std::pair<size_t, size_t> range =
                Removal_Range(source, spans[index], removed_spans);
            // Several removed statements on one line share one whole-line
            // range; emit it once.
            if (std::find(removal_ranges.begin(), removal_ranges.end(), range) ==
                removal_ranges.end()) {
                removal_ranges.push_back(range);
                edits.push_back({range.first, range.second, ""});
            }
        }
        if (desired_count > existing_count) {
            // Appended lines go just past the newline ending the last live
            // enable's line. If `stop` shares that line, they would land
            // after `stop` and the loader would never see them, so refuse
            // instead of writing dead enable lines.
            if (has_stop && !spans.empty() && stop_location.line == spans.back().line) {
                return Fail(diagnostic, stop_location,
                    "config has a 'stop' statement at line " +
                        std::to_string(stop_location.line) +
                        " sharing the line of the last enable line; new enable lines "
                        "appended after it would sit after 'stop' and never take effect; "
                        "refusing to edit it");
            }
            size_t insert_at = source.size();
            std::string inserted;
            if (existing_count > 0) {
                const size_t line_end = source.find('\n', spans.back().end);
                if (line_end != std::string::npos) {
                    insert_at = line_end + 1;
                } else {
                    insert_at = source.size();
                    if (!source.empty() && source.back() != '\n') {
                        inserted += newline;
                    }
                }
            } else if (!source.empty() && source.back() != '\n') {
                inserted += newline;
            }
            for (size_t index = existing_count; index < desired_count; ++index) {
                inserted += "enable " + enabled_mods[index] + ";" + newline;
            }
            edits.push_back({insert_at, insert_at, std::move(inserted)});
        }

        *rewritten = Apply_Edits(source, edits);
        return true;
    }

    bool Rewrite_Setting(const char* source_name, const std::string& source,
        const std::vector<std::string>& setting_path, const std::string& value,
        std::string* rewritten, s_cfg_diagnostic* diagnostic) {
        if (source_name == nullptr || rewritten == nullptr || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        if (setting_path.empty()) {
            return Fail(diagnostic, Location_At(source_name, 0, 1),
                "setting path is empty; refusing to write it");
        }
        for (const std::string& segment : setting_path) {
            if (!Is_Valid_Path_Segment(segment)) {
                return Fail(diagnostic, Location_At(source_name, 0, 1),
                    "setting path segment '" + segment +
                        "' is neither an identifier nor a decimal index; refusing to write it");
            }
        }
        if (!Validate_Setting_Value(source_name, value, diagnostic)) {
            return false;
        }

        s_cfg_document document;
        if (!Parse_Cfg(source_name, source.data(), source.size(), &document, diagnostic)) {
            return false;
        }

        const s_cfg_statement* match = nullptr;
        s_setting_span span;
        bool has_stop = false;
        s_source_location stop_location;
        for (const s_cfg_statement& statement : document.statements) {
            if (statement.kind == CFG_STATEMENT_STOP) {
                has_stop = true;
                stop_location = statement.location;
                break;
            }
            if (statement.kind != CFG_STATEMENT_ASSIGNMENT ||
                !Paths_Equal(statement.path, setting_path)) {
                continue;
            }
            if (match != nullptr) {
                return Fail(diagnostic, statement.location,
                    "setting '" + Join_Setting_Path(setting_path) + "' is assigned twice, at line " +
                        std::to_string(match->location.line) + " and line " +
                        std::to_string(statement.location.line) +
                        "; refusing to edit it until the duplicate is removed by hand");
            }
            match = &statement;
            if (!Find_Setting_Span(source, statement, &span, diagnostic)) {
                return false;
            }
        }

        if (match == nullptr && has_stop) {
            return Fail(diagnostic, stop_location,
                "config has a 'stop' statement at line " +
                    std::to_string(stop_location.line) +
                    " and no assignment to '" + Join_Setting_Path(setting_path) +
                    "'; a new line appended at the end of the file would sit after 'stop' and never take effect; refusing to edit it");
        }

        std::vector<s_edit> edits;
        if (match != nullptr) {
            const std::string current_value = source.substr(span.value_start,
                span.value_end - span.value_start);
            if (current_value != value) {
                edits.push_back({span.value_start, span.value_end, value});
            }
        } else {
            const std::string newline = File_Newline(source);
            std::string inserted;
            if (!source.empty() && source.back() != '\n') {
                inserted += newline;
            }
            inserted += Join_Setting_Path(setting_path) + " = " + value + ";" + newline;
            edits.push_back({source.size(), source.size(), std::move(inserted)});
        }
        *rewritten = Apply_Edits(source, edits);
        return true;
    }

    bool Write_Enabled_Mods(const std::string& path,
        const std::vector<std::string>& enabled_mods, s_cfg_diagnostic* diagnostic) {
        if (path.empty() || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        std::string source;
        std::filesystem::path target;
        s_cfg_document document;
        if (!Read_Config_File(path, &source, &target, &document, diagnostic)) {
            return false;
        }
        if (!Check_Write_Includes(path, target, document, nullptr, diagnostic)) {
            return false;
        }

        std::string rewritten;
        if (!Rewrite_Enabled_Mods(path.c_str(), source, enabled_mods, &rewritten, diagnostic)) {
            return false;
        }
        if (rewritten == source) {
            return true;
        }
        return Write_Rewritten_File(path, target, rewritten, diagnostic);
    }

    bool Write_Setting(const std::string& config_path,
        const std::vector<std::string>& setting_path, const std::string& value,
        s_cfg_diagnostic* diagnostic) {
        if (config_path.empty() || diagnostic == nullptr) {
            return false;
        }
        *diagnostic = {};
        std::string source;
        std::filesystem::path target;
        s_cfg_document document;
        if (!Read_Config_File(config_path, &source, &target, &document, diagnostic)) {
            return false;
        }
        if (!Check_Write_Includes(config_path, target, document, &setting_path,
                diagnostic)) {
            return false;
        }

        std::string rewritten;
        if (!Rewrite_Setting(config_path.c_str(), source, setting_path, value, &rewritten,
                diagnostic)) {
            return false;
        }
        if (rewritten == source) {
            return true;
        }
        return Write_Rewritten_File(config_path, target, rewritten, diagnostic);
    }

} // namespace config
