# The Official Jesse McConnell Game Coding Standard

Version 1.0 — February 3, 2026

## Objective

Balance keeping code human-readable and intuitive with being concise and keeping as
much logic onscreen as possible.

## Priority

1. **Performance first.** Code should be computationally efficient. In no scenario does
   the standard trump performance and optimization: **FastCode > CleanCode**.
2. **Readability second.** Code should be as readable and maintainable as possible while
   maintaining performance.

## Style

### Case

| Kind | Case |
|---|---|
| Global functions or variables | `GLOBAL_FUNCTIONS_OR_VARIABLES` |
| Static functions and variables | `StaticFunctionsAndVariables` |
| Member functions and variables | `MemberFunctionsAndVariables` |
| Local variables | `localVariables` |
| Function parameters | `functionParameters` |

### Naming

**`Register` is for startup and build time only.** It means adding to a table of
content that is a property of the build: rpc ids, field definitions, prefab tables.
Those are registered once, before a session exists, and the table is then frozen.

Something coming alive at runtime is **enlisted** and later **retired**. A body
entering the world is `enlist_body`; leaving is `retire_body`. Using `Register` for
both hides the most important difference between them — one is content both ends of
a connection must agree on before they talk, the other happens freely while they
are talking.

Other runtime verbs, where they fit better: `Subscribe` / `Unsubscribe` for a
listener, `Claim` / `Release` for a pooled index, `Open` / `Close` for a resource.

**Name the fact, not the metaphor.** A reader who does not already know the design
cannot unpack a metaphor, and a vague one reads as two different claims. "The store
is process-wide" was meant as "there is one store for the whole program, not one per
session" but reads just as easily as "the store lasts as long as the process". Say
the first. Prefer "one per program", "one per session", "one per body" — the shape
that actually matters here is nearly always *how many of these exist*.

**A one-word name has to be explicit on its own.** Short is good; vague is not. The
question is whether a reader who does not already know the design can tell what the
call does from the name.

A one-word name passes when the word is an established convention that says the
whole operation. `Bake` means realising dynamic data into a static output, and
`Fingerprint` means a content hash — both fine, because the word carries the meaning
rather than pointing at it. Note that `Fingerprint` is barely shorter than
`ContentHash`, so the metaphor buys nothing there; reach for a literal name unless
the convention is genuinely the clearer one.

It fails when the word depends on a **mechanism the reader cannot see**. `Seal` and
`Freeze` were the failures: they said some blocking would now happen without saying
what blocks, what it blocks, or where that is enforced. When a name would need that
explanation, name the state and the query on it instead:

```cpp
void SetOffset(int offset);
bool IsOffsetSet() const;
```

Two consequences worth stating:

- **Predicates begin `Is`, `Has` or `Can`.** `Writer::Ok()` became `IsOk()`;
  `BodyBits::Test()` became `IsSet()`; `FrameHistory::Holds()` became `HasTick()`.
  A bool-returning *action* is different and needs no prefix — `Host()`, `Join()`
  and `AllocateBlocks()` return whether they succeeded.
- **A convention is only a convention if the reader shares it.** Do not assume one
  carries; if a name leans on a convention from elsewhere, either say so in a word
  or pick the literal name. Check case by case rather than appealing to a list.

**One word, one meaning, across the whole codebase.** If a word already names
something, a second use of it is a bug waiting to be written:

| Word | Reserved for |
|---|---|
| `Handle` | an index into the replicated blocks, one per body |
| `PredictedSlot` | an index into the predicted blocks, one per client |
| `Field` | one entry in the block schema, never a pointer or a struct member |
| `Zone` | a room, and the unit of relevance |
| `Enlist` / `Retire` | a thing coming alive at runtime, and leaving |
| `Claim` / `Release` | taking and returning a pooled index |

Avoid `Kind` and `Type` as members where the thing selects a *destination* rather
than describing a type: a field's `BlockType` says which block it lives in, which
`Kind` did not say at all.

### Curly braces

Curly braces *should* only be used on multiline scopes, including nested scopes such as
loops and if/else statements. At minimum this is a refactor goal.

```cpp
for (int i = 0; i < 10; i++)
    for (int j = 0; j < 10; j++)
        if (i == j)
            if (moreThanOneLine == true)
            {
                j = 0;
                DoStuff();
            }
            else
                break;
```

An `else` binds to the nearest `if`. When nesting unbraced `if`s, brace the outer one
if the `else` is meant for it.

### Comments

Comments are only for explaining:

- external frameworks and API usage
- extremely complex scenarios

Instead of commenting, prefer verbose, self-describing names — especially for
non-predicate functions:

```cpp
LongVerboseNamingConvention();
```

### Spacing

Indentation is 4 columns wide.

Lines in code *should* be broken up into some form of subjective grouping. (The
comments in this example convey the principle and do not themselves follow the
standard.)

```cpp
Class::Class()
{
    // initialization of locals
    int someVar = 0;
    char someOtherVar = 'X';

    // struct creation
    MyStruct myStruct = {};
    myStruct.A = 1;
    myStruct.B = 2;
    myStruct.C = 7;

    // transitioning to logic or function calls
    DoStuff(myStruct.A, myStruct.B);
    DoStuff(myStruct.C, someVar);

    // beginning of scopes
    for (int i = 0; i < someVar; i++)
    {
        DoStuff(i + someVar);
        DoStuff(myStruct.A + myStruct.B);
    }

    // etc.
}
```

### Type inference

Avoid all type inference, except for unresolved objects (generics) or where the
explicit type is an even worse syntax.

Use:

```cpp
for (auto& [handle, allocation] : AllocatedBuffers)
{ ... }
```

instead of:

```cpp
for (std::unordered_map<uint64_t, BufferAllocation>::iterator it = AllocatedBuffers.begin(); it != AllocatedBuffers.end(); ++it)
{ ... }
```

But do not use:

```cpp
int ints[] = { 1, 2, 3, 4, 5 };
for (auto num : ints)
{ ... }
```

Use:

```cpp
for (int num : ints)
{ ... }
```

## Language and engine notes

### C#

- **Indentation** uses tab characters, displayed 4 columns wide.
- **Parameters have no prefix.** `out` and `ref` already mark outputs.
- **Private fields** follow the member rule: `PascalCase`, no underscore.
- **Type inference** covers `var`, target-typed `new()` and collection expressions (`[]`).
  Write the type out on both sides: `List<Item> items = new List<Item>();`. `var` is
  fine where the type is a generic you would otherwise spell out in full, such as
  `foreach (var pair in dictionary)`. The IDE suggests these shorthands as hints; this
  standard overrides them.

### C++ (gdextension)

The extension follows the case table above, not godot-cpp's conventions. Sections are
namespaces under `Skyland` -- `Net`, `Phys`, `Char`, `Asset` -- and each lives in a
folder named for it, so a file's path tells you its namespace.

- **No prefixes on types or parameters.** No `Skn`/`Skp`/`Skc`/`Skm`, and no `p_`,
  `r_` or `io_`. The namespace does the disambiguating, and `const &` versus `&`
  already says whether an argument is written.
- **Bit flags** are `constexpr` groups in a nested namespace (`Layer::Static`,
  `SlotFlag::Active`), so bitwise math needs no casts. **Enumerations** that are pure
  discriminants are `enum class`. **Tuning values** are `constexpr`. All PascalCase: a
  namespaced name is not global.
- **Indentation is 4 spaces**, and braces are Allman, as in the examples above.

The C ABI is the one place a name is not free to follow the rules above, because
something outside the extension resolves it as a string.

- **`extern "C"` exports are `Skyland_PascalCase`** (`Skyland_SharedGet`,
  `Skyland_RpcBroadcast`). The prefix is not for PE, which resolves exports per
  module and so has no collision to avoid -- it is for ELF, where a
  default-visibility symbol goes into a global namespace and a bare `SharedGet`
  could be interposed. Each export is also spelled exactly like the C# method that
  binds it, so the two can be checked against each other by eye:

  ```csharp
  [DllImport(Lib, EntryPoint = "Skyland_SharedGet")]
  internal static extern NetShared* SharedGet(int slot);
  ```

Three things do keep a fixed spelling, each resolved by string from outside:

| Kept | Why |
|---|---|
| Entry symbols (`skyland_net_init`, `skyland_mesh_init`) | Named as strings in the `.gdextension` files. |
| Bound method names (`D_METHOD("get_role")`, signals) | C# reaches them through `Call("get_role")`. The C++ function behind one is PascalCase; only the bound string is fixed. |
| godot-cpp overrides (`_ready`, `_bind_methods`, `_bake_run`) | The engine resolves these by name. |

Renaming an export means renaming its `EntryPoint` string in the same commit. Nothing
looks these up dynamically, so a grep for the old name finds every dependency.

### Godot

- **One Godot script class per file, and the file name must match the class name.** A
  `.tres` refers to its resource's script by file path, and the *New Resource* dialog
  only lists `[GlobalClass]` types it can map to a file. Plain C# classes, structs and
  enums may share a file.
- **Contracts with the C++ extension count as external API** for the comment rule. Enums
  mirrored from C++, wire formats and reserved values (such as id 0) get a comment
  saying what they mirror.
