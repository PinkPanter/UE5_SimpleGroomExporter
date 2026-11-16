# SimpleGroomExporter

> **Fork note.** A fork of [Unity-Technologies/com.unity.demoteam.hair](https://github.com/Unity-Technologies/com.unity.demoteam.hair),
> maintained for [MHBridge Importer](https://assetstore.unity.com/packages/tools/integration/mhbridge-importer-403996).

Export Unreal Engine 5 groom assets to Alembic (`.abc`).

This repository is a fork of [kostenickj/UE5_SimpleGroomExporter](https://github.com/kostenickj/UE5_SimpleGroomExporter) by Jacob Kostenick. The terms this code is available under are recorded in [LICENSE](LICENSE).

## Install

Prebuilt archives are attached to the [releases of this fork](https://github.com/PinkPanter/UE5_SimpleGroomExporter/releases). Download the one that matches your engine version, then extract it into the `Plugins/` folder of your project.

## Export a groom

1. Right-click a groom asset in the Content Browser.
2. Select **Asset Actions > Export**.
3. Set the export options, then click **Export**.

## Export options

| Option | What it does |
|---|---|
| **Use Render Data** | Exports the strands the groom actually renders, with guide interpolation and grooming applied, instead of the raw imported strands. |
| **Merge Hair Groups** | Writes all hair groups into one curves object named `MergedGroom` instead of one object per group. |
| **Axis Preset** | Converts UE axes to Alembic. **Default** writes raw UE coordinates, **For Unity** writes `Alembic(X, Y, Z) = UE(X, Z, Y)`, and **Custom** assigns each output axis individually, with negation. |
| **Export Widths** | Writes the strand diameter into the `width` param. |
| **Export Root UV** | Writes each strand's root UV into the `uv` param. |

## What lands in the file

Both attributes are written at **vertex scope**, so their arrays run parallel to the positions.

**`width`** is a diameter in centimeters, and it is the diameter the groom renders: the per-point
radius, doubled, with the group's **Hair Root Scale** and **Hair Tip Scale** applied along the
strand. Overriding **Hair Width** on the group rescales it so the widest strand matches that number.
A scalp groom lands around `0.007` cm.

**`uv`** is the strand's root UV, repeated on every vertex of that strand. Repeating it costs two
floats per vertex and keeps the array parallel to the positions, which is how readers that size UVs
by vertex count expect to find it.

Positions are written in Unreal units, unconverted, so both attributes share one scale.

With **Merge Hair Groups** on, every group lands in one `MergedGroom` object. If any group lacks
valid width or root UV data, the merged object omits that attribute for all groups, and the
export log names the group that caused it.

## What this fork adds

Upstream writes positions only. This fork adds:

- Coordinate mapping presets, including one tested against the Unity hair pipeline.
- The `width` and `uv` params described in [What lands in the file](#what-lands-in-the-file).
- An option to merge all hair groups into one curves object.
- Export options that persist between sessions.

## Credits

Original exporter by Jacob Kostenick. For questions about upstream, join [his Discord server](https://discord.gg/PvbKTYbKR5).

**Support the ORIGINAL author.** The button below goes to Jacob Kostenick, the author of the
upstream exporter.

[![Support Jacob Kostenick on ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/V7V6YBSIL)
