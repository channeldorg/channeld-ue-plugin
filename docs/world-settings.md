# World Settings
Since v0.7.4, ChanneldUE comes along with a utility class `ChanneldWorldSettings` that can be used to configure the level-specific launch settings in addition to UE's WorldSettings class.

It is useful for the projects that have multiple levels which have different launch settings such as the `Channel Data View Class` and the `Server Groups`. [ChanneldUE Demos](https://github.com/metaworking/channeld-ue-demos) project is a good example. Before the `ChanneldWorldSettings` existed, every time you open a different level, you may need to manually set the `Channel Data View Class` in the Project Settings and the `Server Groups` in the Editor Preferences. Now, you can override these values in `ChanneldWorldSettings` and save them in the level asset.

To use `ChanneldWorldSettings`, open the Project Settings and navigate to `Engine > General Settings`. Under the `Default Classes` category, set the `World Settings Class` to `ChanneldWorldSettings`. In order to see the changes, you need to restart the editor.

Below is a screenshot of the World Settings window that corresponds to the `TestSpatial_2x2` map in the ChanneldUE Demos project:

![](images/channeld_world_settings.png)

The `ChanneldWorldSettings` class has the following properties:

- `Channel Data View Class Override`: It overrides the `Channel Data View Class` in the Project Settings.
- `Server Launch Groups Override`: It overrides the *whole array* of the `Server Groups` in the Editor Preferences.
- `Channeld Launch Parameters Override`: It only overrides the parameters that have the *same key* in the `Channeld Launch Parameters` in the Editor Preferences. If the key does not exist in the Editor Preferences, it will be added to the `Channeld Launch Parameters`.

## Remove the duplicate World Settings actors in level
After setting the `World Settings Class` to `ChanneldWorldSettings`, it may happen that there are two `World Settings` in the level asset. If that is the case, the log will give the following warning in UE4:
```log
LogLevel:  Detected duplicate WorldSettings actor - UE-62934
```
and in UE5:
```log
LogWorld: Warning: Extra World Settings '/Game/Maps/<LevelName>:PersistentLevel.WorldSettings_0' actor found. Resave level to clean up.
```

To remove the duplicate World Settings actors, you can use this tool in ChanneldUE: in the drop-down menu of the ChanneldUE toolbar, select `Advanced... > Remove Duplicate World Settings Actors`. This action saves the current level asset automatically.
