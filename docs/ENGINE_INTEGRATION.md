# Game Engine Integration Guide

[← Back to Home](../README.md) | [简体中文](./ENGINE_INTEGRATION_CHS.md)

---

## Unity / Tuanjie Engine

### Integration

- **Unity**
  - Download `unity_package_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases);
  - Unzip and in Unity Package Manager select "Install from tarball", pointing to the `.tar` file inside to import;
  - Official Unity does not support HarmonyOS yet; if you need HarmonyOS support, you can integrate it yourself as needed.

- **Tuanjie Engine**
  - Download `tuanjie_package_{version}` from the [Releases page](https://github.com/Tencent/BqLog/releases);
  - Unzip and import via Unity Package Manager as tarball similarly;
  - The main difference from Unity is that HarmonyOS related support is already integrated.

### Usage

After importing, use the `bq.log` C# API:

```csharp
string config = @"
    appenders_config.console.type=console
    appenders_config.console.levels=[all]
";
var log = bq.log.create_log("cs_log", config);
log.info("Hello from Unity! value:{}", 42);
```

For more examples, refer to `/demo/csharp` directory in the repository.

---

## Unreal Engine

Pick any one of the following ways to integrate.

### Supported engine versions

| Install method | Supported Unreal Engine versions |
|---|---|
| **Fab** | UE 4.22 – 4.27 and UE 5.0 – 5.8 (one dedicated zip per officially available engine minor) |
| **Prebuilt (GitHub Release)** | UE 4.25 – 4.27 (`ue4` zip), UE 5.0 – 5.8 (`ue5` zip), and current UE6 development builds (`ue6` zip) |
| **Source (GitHub Release)** | UE 4.25 – 4.27 (`ue4` zip), UE 5.0 – 5.8 (`ue5` zip), and current UE6 development builds (`ue6` zip) |

> **On UE 4.22 / 4.23 / 4.24?** Install through [Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf) — the per-engine-version Fab zips account for `.uplugin` token differences in those older UE4 minors (older UBT revisions don't recognize `UncookedOnly` or `LinuxAArch64`). The single GitHub `ue4` zip targets UE 4.25+.
>
> **Mac / iOS on UE 4.22 and 4.23:** Due to Fab build-machine limitations, the Fab zips for UE 4.22 and 4.23 do **not** include Mac or iOS binaries. If you need Mac or iOS support on these two engine versions, please download the **Source** or **Prebuilt** package from the [GitHub Releases page](https://github.com/Tencent/BqLog/releases) and integrate it manually.
>
> **Using UE6?** Download the `ue6` Source or Prebuilt plugin manually from the [GitHub Releases page](https://github.com/Tencent/BqLog/releases). The package holds two plugins: `BqLog`, the same plugin as for UE5 with the code paths UE6 needs, and the optional `BqLogVerse`, which adds a Verse API (see [Using BqLog in Verse](#4-using-bqlog-in-verse-ue6)). Both have been verified with current UE6 development builds. UE6 is still under development, so compatibility may be adjusted as the engine changes. Fab cannot distribute the UE6 package yet because Epic's official Fab engine support currently ends at UE 5.8.
>
> **On PS4 / PS5 / Switch?** Use the **Source** package: the prebuilt one ships no console binaries. The plugin injects the console platform macros and sets both base dirs itself. Platform setup is in [Integration Guide — console platforms](./INTEGRATION_GUIDE.md#console-platforms).

### Integration

- **Fab (recommended)**
  - Install from the Epic Fab listing: [BqLog on Fab](https://www.fab.com/listings/386d1c78-e164-4e97-8b3e-e88cbf9b6acf);
  - Use the Epic Games Launcher / Fab client to add the plugin to your engine, then enable it in your project — no manual download or unzip required;
  - Recommended when you want the easiest installation and automatic updates through Fab.

- **Prebuilt**
  - Download `unreal_plugin_prebuilt_ue4_{version}` (UE 4.25 – 4.27), `unreal_plugin_prebuilt_ue5_{version}` (UE 5.0 – 5.8), or `unreal_plugin_prebuilt_ue6_{version}` (current UE6 development builds) from the [Releases page](https://github.com/Tencent/BqLog/releases);
  - Unzip to the `Plugins` directory of your game project.

- **Source**
  - Download `unreal_plugin_sources_ue4_{version}` (UE 4.25 – 4.27), `unreal_plugin_sources_ue5_{version}` (UE 5.0 – 5.8), or `unreal_plugin_sources_ue6_{version}` (current UE6 development builds) from the [Releases page](https://github.com/Tencent/BqLog/releases);
  - Unzip to the `Plugins` directory of your game project, to be recompiled by the engine.

After extraction, verify that the plugin descriptor is at the following path. An extra nested `BqLog` directory will prevent Unreal from discovering the plugin correctly.

```text
<YourProject>/Plugins/BqLog/BqLog.uplugin
```

### Enabling the Plugin

Add the BqLog plugin to your `.uproject` file's `Plugins` section:

```json
"Plugins": [
    {
        "Name": "BqLog",
        "Enabled": true
    }
]
```

> Merge this entry into an existing `Plugins` array rather than adding a second `Plugins` key. Plugins placed in the project `Plugins` directory are usually discovered automatically, but explicitly declaring BqLog is recommended for clarity and version control. No `.uproject` `AdditionalDependencies` entry is required.

### Adding BqLog Module Dependency

In every game module that includes BqLog headers, add `"BqLog"` to that module's `.Build.cs`. If BqLog is used only by `.cpp` files, a private dependency is preferred:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] {
    "BqLog"
});
```

Use `PublicDependencyModuleNames` instead if one of your module's public headers exposes BqLog types or includes a BqLog header:

```csharp
PublicDependencyModuleNames.Add("BqLog");
```

You can then include the Unreal adapter from C++:

```cpp
#include "BqLog.h"
```

> **Inside Unreal Engine, include `"BqLog.h"`, not `"bq_log/bq_log.h"`** — the latter carries no Unreal adapter. **Also do not** add include directories manually: the `BqLog` module exports the required paths. The Blueprint node module `BqLogBPNodes` is editor-only and does **not** need to be added to your dependencies. After changing `.uproject` or `.Build.cs`, regenerate project files if your IDE does not refresh automatically, then rebuild the Editor target.

### 1) Support for `FName` / `FString` / `FText`

In Unreal environment, BqLog has built-in adapters:

- Automatically support `FString`, `FName`, `FText` as format string and parameters;
- Compatible with UE4, UE5, and current UE6 development builds.

Example:

```cpp
bq::log log_my = bq::log::create_log("AAA", config);   // config omitted

FString fstring_1 = TEXT("This is a test FString{}");
FString fstring_2 = TEXT("This is also a test FString");
log_my.error(fstring_1, fstring_2);

FText text1 = FText::FromString(TEXT("This is a FText!"));
FName name1 = FName(TEXT("This is a FName"));
log_my.error(fstring_1, text1);
log_my.error(fstring_1, name1);
```

If you wish to customize adaptation behavior, you can also use "Method 2 (Global Function)" to define `bq_log_format_str_size` and `bq_log_format_str_chars` yourself (see [Advanced Usage](./ADVANCED_USAGE.md)).

### 2) Redirect BqLog output to Unreal Log Window

The Unreal plugin automatically redirects all BqLog output to Unreal's Output Log — no extra setup is required.

> **Note:** If you are integrating BqLog at the C++ level without the plugin, you can use `bq::log::register_console_callback` to forward logs to `UE_LOG` manually. See [Advanced Usage](./ADVANCED_USAGE.md) for details.

### 3) Using BqLog in Blueprint

After importing the Unreal plugin, BqLog can be called directly in Blueprint:

1. **Create Log Data Asset**

   - Create Data Asset in Unreal project, type select BqLog:
     - Default Log Type (without Category):
       <img src="img/ue_pick_data_asset_1.png" alt="Default Log Creation" style="width: 455px">
     - If Log Class with Category is generated (see [Advanced Usage — Category](./ADVANCED_USAGE.md)), and `{category}.h` and `{category}_for_UE.h` are added to project:
       <img src="img/ue_pick_data_asset_2.png" alt="Category Log Creation" style="width: 455px">

2. **Configure Log Parameters**

   - Double click to open Data Asset, configure log object name and creation method:
     - `Create New Log`: Create a new Log object at runtime:
       <img src="img/ue_create_log_config_1.png" alt="Config Log Params Create New Log" style="width: 455px">
     - `Get Log By Name`: Only get Log with same name created elsewhere:
       <img src="img/ue_create_log_config_2.png" alt="Config Log Params Get Log By Name" style="width: 455px">

3. **Call Log Node in Blueprint**

   <img src="img/ue_print_log.png" alt="Blueprint Call Log" style="width: 655px">

   - Area 1: Add log parameters;
   - Area 2: Added log parameter nodes, can be deleted via right click menu (Remove ArgX);
   - Area 3: Select log object (i.e. Data Asset just created);
   - Area 4: Displayed only when log object has Category, can select log Category.

4. **Test**

   - Run Blueprint, if configured correctly and there is ConsoleAppender output, similar output can be seen in Log window:

     ```text
     LogBqLog: Display: [Bussiness_Log_Obj] UTC+7 2025-11-27 14:49:19.381[tid-27732 ] [I] [Factory.People.Manager] Test Log Arg0:String Arg, Arg1:TRUE, Arg2:1.000000,0.000000,0.000000|2.999996,0.00[...]
     ```

### 4) Using BqLog in Verse (UE6)

The `ue6` package also contains `BqLogVerse`, a plugin that brings BqLog to Verse with the API of the other languages: the names are those of C++ (`bq.log` is `bq::log`, `bq.create_log` is `bq::log::create_log`), and a category log is written to as `MyLog.info(MyLog.cat.Shop.Seller, "...")`. Verse can create and configure logs, and use the ones C++, Blueprint or a BqLog Data Asset created. It is experimental and disabled by default, so a project that does not use Verse is not affected by it.

1. **Enable it**: keep `<YourProject>/Plugins/BqLogVerse/BqLogVerse.uplugin` next to `Plugins/BqLog`, and add it to the `.uproject`:

   ```json
   "Plugins": [
       { "Name": "BqLog", "Enabled": true },
       { "Name": "BqLogVerse", "Enabled": true }
   ]
   ```

   A plugin that holds your Verse code lists `BqLogVerse` among its own `Plugins` dependencies. `BqLogVerse` itself depends on the engine's `Verse` and `Solaris` plugins.

2. **Write from Verse**: `bq.create_log` creates a log from the same configuration text as `bq::log::create_log`, and `bq.get_log_by_name` finds a log by name, whoever created it.

   ```verse
   using { /Tencent.com/BqLog }

   game_logic := class:
       Start()<transacts>:void =
           Config := "appenders_config.file.type=compressed_file\nappenders_config.file.file_name=Game\n"
           if (GameLog := bq.create_log["Game", Config]):
               GameLog.info("game started")

       OnJoin(Name:string)<transacts>:void =
           # Any code can find a log by name, whoever created it: Verse, C++, Blueprint or a BqLog Data Asset.
           if (GameLog := bq.get_log_by_name["Game"]):
               GameLog.debug("player {Name} joined")
   ```

3. **Category logs**: `BqLog_CategoryLogGenerator` also writes `<ClassName>.verse` (see [Category log class generation](ADVANCED_USAGE.md#category-log-class-generation)). Put it in a Verse folder of your project or plugin; it defines the module `<ClassName>` there, with the same categories as the C++ class, used as in the other languages:

   ```verse
   if (MyLog := business_log.create_log["MyLog", Config]):
       MyLog.info("in the default category")
       MyLog.info(MyLog.cat.Shop.Seller, "in category Shop.Seller")

   # Elsewhere: the same log, whoever created it (Verse, business_log::create_log in C++, Blueprint or a BqLog Data Asset).
   if (MyLog := business_log.get_log_by_name["MyLog"]):
       MyLog.warning(MyLog.cat.Shop, "found it")
   ```

   `business_log.log` derives from `bq.category_log`, which derives from `bq.log`, so it goes wherever a `bq.log` is expected. `business_log.get_log_by_name` fails if the log's categories are not those of the class. A category whose name Verse already uses, such as a keyword, a name of `/Verse.org/Verse` like `Log`, or a definition of the generated module like `log`, becomes a field ending in `_` (`MyLog.cat.Log_`), and the comment beside the field gives the category.

| Verse | Meaning |
|---|---|
| `bq.create_log[Name, Config]` | Creates the log named `Name` from `Config`, like `bq::log::create_log`. If a log of that name exists, `Config` is applied to it instead; an empty `Name` gets a generated one, which `get_name()` of the result returns. Fails if `Config` cannot be parsed, or if the existing log has categories |
| `bq.get_log_by_name[Name]` | The log named `Name`, whoever created it. A log with categories is found too, and written to in its default category. Fails if there is none |
| `<ClassName>.create_log[Name, Config]`, `<ClassName>.get_log_by_name[Name]` | The same for a category log class. Both fail if the log of that name has other categories |
| `verbose(Message)` … `fatal(Message)` | Writes `Message` at that level, in the default category |
| `verbose(Category, Message)` … `fatal(Category, Message)` | Category log: writes `Message` in `Category`, such as `MyLog.cat.Shop.Seller` |
| `is_valid[]`, `get_name()`, `get_id()` | Like `is_valid`, `get_name` and `get_id` in C++; `get_id()` returns the id C++ sees |
| `reset_config[Config]`, `set_appender_enable(AppenderName, Enable)`, `force_flush()`, `take_snapshot(TimeZone)` | Like the C++ functions of the same names. `reset_config` fails if `Config` cannot be parsed |
| `get_categories_count()`, `get_categories_name_array()` | Category log: the number of categories, and their names in index order with the default category `""` first |
| `bq.force_flush_all_logs()`, `bq.get_version()` | Like `bq::log::force_flush_all_logs` and `bq::log::get_version` |

Behaviour to know about:

- **The message is not a format string.** Verse's own interpolation (`"{Name}"`) has already produced the text, so braces that reach BqLog are written as they are.
- **Nothing here is transactional.** The level functions are `<computes>`, and `create_log`, `reset_config` and `set_appender_enable` are `<transacts>`, so Verse lets you call them from transactional code, but each takes effect at once and stays if the enclosing Verse transaction is rolled back.
- **Verse paths.** The API lives at `/Tencent.com/BqLog`. Its package is an `InternalUser` package because Verse accepts `<native>` declarations only there; your own Verse packages stay `PublicUser` and use it like any other module.

---

## Next Steps

- [Integration Guide](./INTEGRATION_GUIDE.md) — Integration for all platforms and languages
- [API Reference](./API_REFERENCE.md) — Core APIs for creating logs, writing logs, and more
- [Advanced Usage](./ADVANCED_USAGE.md) — Category, custom types, encryption, and more
