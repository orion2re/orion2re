# Authoring Mods: JSON5 String Overrides

Version 2.00 supports individual string replacements in selected LBX resources. A package can supply a JSON5 text file instead of replacing a complete binary file or LBX entry. Include only the strings the mod changes; omitted IDs retain the text supplied by eligible lower-priority sidecars or the underlying LBX.

## Minimal string override

Create and enable a [package](packages-and-manifests.md), then put this file at `data/mods/example_mod/data/estrings.json5`:

```json5
{
  // String IDs start at 1; omitted strings keep lower-priority text.
  "2": " %s gains a level.",
}
```

This replaces string 2 in `estrings.lbx`. The leading space and `%s` placeholder are part of the string; preserve placeholders expected by the game. Other strings remain unchanged.

In a ZIP package, the file belongs at `data/estrings.json5`, with `mod.json5` at archive root. For a loose development override, use `data/overrides/estrings.json5`; no manifest or `enable` statement is needed there.

## Filenames and languages

The matching JSON5 file is called a sidecar. Its name replaces `.lbx` with `.json5`, retaining any directory component.
The actual English H-string filename is `hestrngs.lbx`, so its sidecar is
`hestrngs.json5` (not `hestrings.json5`). Localized LBX filenames have their own
matching sidecars. Files containing several languages use logical string IDs
independent of the selected language.

## Precedence and reloading

Standard [VFS precedence](vfs-and-assets.md#resolution-order) applies, including loose overrides and mounted packages.
A complete LBX file in a higher-priority layer suppresses the matching JSON5
sidecar from lower-priority layers. For example, VDC's `techdesc.lbx` takes
precedence over v150's `techdesc.json5` when VDC is above v150. A sidecar at the
same priority as its LBX, or above it, still replaces the listed strings. This
rule is rechecked when strings are loaded after
mounting, unmounting, or reordering packages.

Eligible sidecars are merged per string ID. For example, if a higher-priority mod's
`estrings.json5` changes ID 1 and a lower-priority mod's sidecar changes ID 2, both
replacements apply. When several sidecars define the same ID, the highest-priority
value wins, including an explicitly empty string where the resource permits it.
IDs omitted by all eligible sidecars retain their LBX text. Sidecars below a
complete-LBX replacement are not eligible, even for IDs omitted by higher sidecars.

The underlying LBX remains required; a sidecar is not a standalone replacement
for missing game data. A missing sidecar simply leaves the LBX text unchanged.
Changes to sidecar contents or VFS precedence are detected when strings are next
loaded. Invalid replacements report an error rather than using stale cached text.

Changes take effect when the corresponding strings are next loaded, not when
already-resident strings are drawn. Packed tables still need their normal reload.
ZIP changes are visible only when VFS exposes the updated archive contents; this
does not add archive watching or remounting.

## Syntax, encoding, and limits

JSON5 comments, single quotes, and trailing commas are supported. Bare decimal
keys are an intentional extension to JSON5, which normally requires numeric keys
to be quoted. IDs must be positive decimal integers and values must be strings;
duplicate IDs within one file are errors. The same ID in different sidecars follows
the priority rule above. Unused IDs do not add strings and have no effect.
Files are limited to 4 MiB and must be valid UTF-8 JSON5. String values represent game bytes, not Unicode display text: each decoded character from `U+0001` through `U+00FF` supplies the byte with the same numeric value. There is no CP850 conversion or encoding flag. Embedded NUL and characters above `U+00FF` are errors.

ASCII text stays unchanged. Use `\u00XX` or JSON5's `\xXX` escapes for high bytes and control codes. For example, `\u0090` supplies byte `0x90`, and `\u0080` supplies byte `0x80`. A literal character with value `U+00E9` supplies byte `0xE9`, not CP850 byte `0x82`.

The meaning of a byte depends on the resource and selected language. Localized fonts also use ASCII positions for accented glyphs; preserve spellings such as German `n[chste`, French `sup#rieur`, and Italian `perch#` when copying original resources. Spanish `\u0090poca de Galaxia` preserves the original leading byte `0x90`.

When extracting an LBX string, copy ASCII and escape other bytes by their numeric values. Also escape quotes, backslashes, and control characters as JSON5 requires. Do not paste an arbitrary binary byte stream into a UTF-8 file or convert the resource through a display code page first.

Overrides written for the earlier CP850 conversion must use the original byte values instead. For example, an event token previously written as `\u00c7` must now be `\u0080`. ASCII-only overrides need no changes.

Packed string tables store several strings together in one LBX entry. These tables can grow beyond their original entry size; JSON5 files do not need binary padding.

Some strings, such as ship names, still have fixed length limits. Replacements must fit those limits, including the ending NUL byte added by the game; values are not silently truncated. Technology and race tables cannot contain empty replacement strings.

Preserve format placeholders such as `%s` and `%d`, and any control codes expected by the game. The engine checks the override file's syntax and encoding, but does not check whether placeholders or substitution tokens match the values supplied by the game.

See [Text formatting](text-formatting.md) for layout and substitution rules.
Use JSON escapes such as `\n` for newlines and `\\` for a literal backslash.
Event messages also contain special substitution bytes. Write their original
numeric values directly, such as `\u0080` for the colony-name token. Do not
remove or translate these tokens.

## Supported resources and string IDs

All numbers below are sidecar IDs, starting at 1. Source indices start at 0.

| LBX files (without `.lbx`) | Numbering |
| --- | --- |
| `estrings`, `estrGERM`, `estrFREN`, `estrSPAN`, `estrITAL`, `estrPOLI` | 1-812, packed table order. Slot 369 remains the game-selected main-text filename. |
| `HESTRNGS`, `HGSTRNGS`, `HFSTRNGS`, `HSSTRNGS`, `HISTRNGS` | 1-397, packed table order. |
| `rstring0` through `rstring5` | 1-177 across the complete packed table. |
| `billtext`, `billtex2`, `jimtext`, `jimtext2`, `KENTEXT`, `KENTEXT1` | Logical message index + 1, not physical entry + 1. Six language entries share an ID. KENTEXT1 starts at 1 for KEN message index 80. |
| `EVENTMSE`, `EVENTMSG`, `EVENTMSF`, `EVENTMSS`, `EVENTMSI`, `EVENTMSP` | `event_type * 4 + event_subtype + 1`. |
| `antarmsg` | 1-8: two message sets, four alternatives each. |
| `councmsg` | 1-14, excluding the language block offset. |
| `maintext`, `maingerm`, `mainfren`, `mainspan`, `mainital`, `mainpoli` | Description entry + 1. |
| `techname` | 1-560, packed table order within the selected language. Existing explicit configuration name overrides still take precedence. |
| `RACESTUF` | 1-64 option strings; 65-96 special-ability names. Binary trait entries are untouched. Dynamically generated primary-pick labels still replace their corresponding LBX labels. |
| `SKILDESC`, `GERSKLLS`, `FRESKLLS`, `SPASKLLS`, `ITASKLLS` | 1-27 skill names; 28-54 descriptions. |
| `TECHDESC`, `GERTECD`, `FRETECD`, `SPATECD`, `ITATECD` | 1-39 special descriptions (entry 1); 40-79 weapon notes (entry 3). Weapon names in entry 2 are not covered. |
| `shipname` | Record index + 1. |
| `RACENAME` | Record index + 1, eight candidate names per race. |
| `starname` | 1-13 home-system names; 14-842 random star names. |
| `CREDITS`, `GERCRDTS`, `FRECRDTS`, `SPACRDTS`, `ITACRDTS` | Display lines in order, two per credit. The leading record containing the credit count is not overridable. |

## Unsupported resources

JSON5 sidecars do not apply to arbitrary LBX entries. Mixed binary records such
as `HELP` (including localized variants), `DIPLOMS*`, and `HERODAT*` contain text
alongside metadata and are not supported. `ENGMSG`, `MSGENG`, and `CUSTMSTR` also
have no active string-override support.

Use [binary asset overrides](vfs-and-assets.md) for resources outside the table
above; merely creating a matching JSON5 filename does not enable support.

## Testing a string override

1. Confirm the package is enabled and the sidecar is below its `data/` directory.
2. Check the resource-specific ID mapping above; an LBX entry number is not
   necessarily a string ID.
3. Remove loose development overrides that could override the string IDs being tested.
4. Reload the relevant resource or restart the game, then open the affected screen.
5. Check diagnostics for invalid IDs, encoding, or strings that exceed fixed fields.
6. Test substitutions with real game data, including plural forms and line wrapping.
7. Test the packaged ZIP without loose development copies.

The bundled v150 mod's English JSON5 files under `data/mods/v150/data/` provide
examples of sparse overrides, including event substitution tokens. See also
[Testing and troubleshooting](testing.md).