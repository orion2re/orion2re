# Authoring Mods: Configuration Overrides

CFG is the hand-authored language for changing fields exposed by version 2.00. It is schema-aware: paths, types, accepted names, ranges, defaults, array indexes, and record order come from the engine's generated configuration schema.

Use the [configuration reference](../generated/configuration/index.md) that matches the game release. Do not copy a 1.50 name or infer a field from native source layout.

## Statements

Basic statements are:

```text
# Comments continue to the end of the line.
path to field = value;
include relative/file.cfg;
enable another_package;
stop;
```

Every statement ends with a semicolon. Paths use whitespace, not dots or brackets:

```text
ships guardian weapons 1 amount = 3;
combat fighters_use_point_defense = true;
```

The manifest-selected CFG entry point may include other files. Relative include paths are resolved from the file containing the statement. Missing files, include cycles, excessive nesting, and errors in included files stop package activation. `stop;` ends the current file and returns to its includer.

## Values

CFG supports:

- decimal, hexadecimal, and binary integers: `18`, `0x12`, `0b10010`;
- symbolic enum values and Booleans: `death_ray`, `true`, `false`;
- comma-separated masks: `hv,eccm`;
- exact ratios: `1/3`;
- quoted strings with JSON-style escapes; and
- `-` as a skip in a sequential assignment.

The selected schema field determines which forms are valid. Prefer symbolic names when the reference provides them. Numeric values are not a way to bypass a finite enum or mask domain.

## One-field assignments

An explicit leaf is the clearest form:

```text
ships guardian weapons 1 amount = 3;
ships guardian weapons 1 mods = hv,eccm;
```

Indexes follow the field definition. Some arrays are zero-based, others one-based, and some accept semantic names. Use the matching generated reference rather than assuming an index base.

## Records and sequential assignments

An aggregate assignment supplies writable leaves in declared order. For a weapon record ordered as type, arc, mods, ammo, and amount:

```text
ships guardian weapons 1 = death_ray all_sectors hv 0 2;
```

Use `-` to preserve one position:

```text
# Keep type and arc, change mods, keep ammo, change amount.
ships guardian weapons 1 = - - hv,eccm - 3;
```

A scalar can start a bounded suffix assignment:

```text
ships guardian weapons 1 mods = hv,eccm 0 3;
```

This updates `mods`, `ammo`, and `amount` in weapon 1. It cannot spill into weapon 2. Selecting the complete `weapons` array instead permits traversal across its elements but not into the following field.

The complete statement is decoded and validated before any of its writes are committed. Too many values, unknown fields, invalid enum names, out-of-range numbers, and an assignment containing only skips are errors.

## Labeled groups

Labeled groups make sparse record changes without repeating the common path:

```text
ships guardian weapons 1 =
    type: death_ray
    mods: hv,eccm
    amount: 3;
```

Each group acts like a scalar-start assignment relative to the common path. Duplicate labels and groups whose write ranges overlap are rejected. A statement is either positional or labeled; do not mix unlabeled values before the first group.

Use explicit leaf assignments for maintainability. Use positional records and tables when the matching schema order is intentional and the compact form is useful.

## Arrays, maps, and vectors

Selecting an array permits ordered traversal over the selected array. Selecting one record-valued element confines the assignment to that element. Maps use their declared key order and may accept schema-defined names. Vectors use zero-based indexes and may grow when an assignment targets a later element; growth commits only when the complete statement validates.

The [allowed values](../generated/configuration/allowed-values.md) page lists current symbolic domains. [Structured setting fields](../generated/configuration/record-layouts.md) lists record order for compact assignments.

## Package order and collision behavior

Package CFG files are applied from lowest to highest package priority. The first explicit `enable` therefore applies last and wins when two packages assign the same field. Package CFG values also override assignments in the base `data/orion2re.cfg`.

Do not rely on accidental file enumeration. Express required package relations in the manifest and required statement order through explicit includes.

## What CFG cannot do

CFG changes only fields exposed by the current schema. It cannot:

- address native memory or source symbols;
- create arbitrary new schema fields;
- run conditional logic or loops;
- replace a read-only asset;
- register an event or callback; or
- reproduce an unexposed 1.50 patch variable by using its old name.

Use VFS assets or Lua when those mechanisms match the requirement. If no public mechanism can express a feature, the engine needs a reviewed semantic extension rather than an undocumented configuration path.

## Validation and diagnostics

Configuration loading is transactional at startup. Invalid syntax, paths, values, package relationships, resources, or cross-field constraints reject the candidate configuration. Diagnostics normally identify the source file, line and column, canonical path, expected type, and structural boundary.

When debugging a package CFG:

1. Start with an empty entry point and confirm that the manifest loads.
2. Add includes and assignments in small groups.
3. Use explicit leaf assignments until the values are known to work.
4. Check the generated reference from the exact same game build.
5. Test the package in its real priority position, not only by itself.
