"""Read-only structural export validation, run with UE's PythonScript commandlet.

Defaults match Export-BakeMap.ps1. Override with RVT_BAKE_MANIFEST environment variable.
Does not generate PCG, bake SVT, or save source/destination assets.
"""
import json
import os
import re
from pathlib import Path

import unreal


manifest_path = Path(os.environ.get(
    "RVT_BAKE_MANIFEST",
    str(Path(unreal.Paths.project_saved_dir()) / "RVTBakeExport/BakeTest_RVTBake.json")))
manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
assert manifest.get("exportSucceeded"), "Manifest does not confirm a completed export"
selected_names = {entry["name"] for entry in manifest["actors"]}


def normalize(value):
    if isinstance(value, (unreal.Array, list, tuple)):
        return [normalize(item) for item in value]
    if isinstance(value, unreal.Object):
        path = value.get_path_name()
        for world_name in sorted((manifest["source"], manifest["destination"]), key=len, reverse=True):
            path = path.replace(world_name + "." + world_name.rsplit("/", 1)[1], "<WORLD>")
            path = path.replace(world_name, "<MAP>")
            path = path.replace(world_name.replace("/Game/", "/Game/__ExternalActors__/"), "<EXTERNAL>")
        return path
    return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))


def property_snapshot(obj, properties):
    return {key: normalize(obj.get_editor_property(key)) for key in properties}


def snapshot(map_path, require_exact_selection):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    assert world, "Could not load " + map_path
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    if isinstance(descs, tuple):
        assert descs[0]
        descs = descs[1]
    names = {str(desc.name) for desc in descs}
    assert selected_names <= names, "Missing exported actors: " + str(selected_names - names)
    if require_exact_selection:
        assert names == selected_names, "Unexpected exported actors: " + str(names - selected_names)
    unreal.WorldPartitionBlueprintLibrary.load_actors([
        desc.guid for desc in descs if str(desc.name) in selected_names])
    actors = {actor.get_name(): actor for actor in
              unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if actor.get_name() in selected_names}
    assert set(actors) == selected_names, "Selected actor could not be loaded"
    result = {}
    for name, actor in actors.items():
        entry = {"class": actor.get_class().get_path_name(),
                 "location": normalize(actor.get_actor_location()),
                 "rotation": normalize(actor.get_actor_rotation()),
                 "scale": normalize(actor.get_actor_scale3d())}
        if isinstance(actor, unreal.LandscapeProxy):
            entry["landscape"] = property_snapshot(actor, ["LandscapeMaterial"])
            entry["bounds"] = normalize(actor.get_actor_bounds(False))
            components = actor.get_components_by_class(unreal.LandscapeComponent)
            entry["components"] = [{"grid": property_snapshot(comp, [
                "SectionBaseX", "SectionBaseY"]),
                "transform": normalize(comp.get_world_transform()),
                "bounds": normalize(unreal.SystemLibrary.get_component_bounds(comp))}
                for comp in sorted(components, key=lambda item: item.get_name())]
            if isinstance(actor, unreal.Landscape):
                entry["editLayers"] = [{"name": str(layer.get_name_bp()),
                    "class": layer.get_class().get_path_name(),
                    "settings": property_snapshot(layer, [
                        "bVisible", "HeightmapAlpha", "WeightmapAlpha"])}
                    for layer in actor.get_edit_layers_bp()]
        if isinstance(actor, unreal.RuntimeVirtualTextureVolume):
            component = actor.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
            entry["rvt"] = property_snapshot(component, [
                "VirtualTexture", "StreamingTexture", "StreamLowMips"])
        pcg_components = actor.get_components_by_class(unreal.PCGComponent)
        if pcg_components:
            entry["pcg"] = [property_snapshot(comp, [
                "GraphInstance", "GenerationTrigger", "bIsComponentPartitioned"])
                for comp in pcg_components]
            entry["pcgGraphs"] = [normalize(comp.get_graph()) for comp in pcg_components]
        result[name] = entry
    return result, len(descs)


source, source_count = snapshot(manifest["source"], False)
destination, destination_count = snapshot(manifest["destination"], True)
differences = [name for name in selected_names if source[name] != destination[name]]
report = {"sourceActorCount": source_count, "destinationActorCount": destination_count,
          "checkedActorCount": len(selected_names), "differences": differences,
          "source": source, "destination": destination}
report_path = manifest_path.with_name(manifest_path.stem + "-validation.json")
report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
assert not differences, "Actor structure/settings differ: " + str(differences)
unreal.log("RVT_BAKE_EXPORT_VALIDATION_OK: {} actors; {} -> {} descriptors".format(
    len(selected_names), source_count, destination_count))
