# Troubleshooting
## Setup script did not download channeld
If you have configured the environment variable `%CHANNELD_PATH%` before running Setup, the Setup script will skip the download and installation steps. The plugin will run channeld according to the environment variable `%CHANNELD_PATH%`.

If you need to download and install channeld to the plugin directory, please delete the environment variable `%CHANNELD_PATH%` before running Setup.

## Unable to start channeld service
- Check if the channeld process exists in the task manager. If it exists, please manually "End Task"
- If the output log shows `failed to listen`, it means that the port is occupied. Please check if the default ports 12108 and 11288 are occupied by other processes, or modify the port number in the channeld configuration file

## The game server automatically exits after startup
- Check if the channeld service is running normally. When the channeld networking (`Enable Channeld Networking`) is enabled, the game server will try to connect to the channeld service. If the connection fails, the game server will automatically exit;
- Make sure that [Live Coding](https://docs.unrealengine.com/5.0/en-US/using-live-coding-to-recompile-unreal-engine-applications-at-runtime/) is turned off, otherwise `Error: Failed to register channel data type by name` will appear in the log;
- If the above methods still cannot solve the problem, please check the log of the game server. The log file is usually named after the project name_{number} under the project directory. In single server mode, the number 2 is the game server log; in multi-server mode, the number 2 is the main server log, and the number starts from 3 is the space server log.

## Unable to save Blueprint
If the error message "Unable to save asset" appears, it is usually because the game server is still running, causing the Blueprint file to be occupied. Please stop the game server before saving the Blueprint.

## The second PIE client cannot enter the scene
Check the log of the UE server. If the following information appears:
```log
LogNetTraffic: Error: Received channel open command for channel that was already opened locally.
```
Please check `Edit -> Editor Preferences -> Level Editor -> Play -> Multiplayer Options` to make sure that `Run Under One Process` is **unchecked**.

## Replication issue
- Make sure to use the latest generated replication code
- Sometimes, the generated replication code is not hot-reloaded after compilation. In this case, you need to recompile and start the UE editor.
- Check the log of the game server to see if there are any synchronization related error messages

## Missing actor state after refreshing replication cache
If the state of the replicated actor doesn't appear in the `Add State` context menu in the Channel Data Schema editor, try deleting the `Intermediate` directory under the project directory, then recompile the project, start the UE editor. This should solve the problem.

## The generated replication code failed to compile
This problem may be caused by the branch switching of the ChanneldUE plugin. The solution is to manually delete the `Source/<Project Name>/ChanneldGenerated` directory under the project directory, then recompile the project, start the UE editor, and generate the replication code again.

## The color of the outline is incorrect after the character crosses the server
Check if the spatial servers are running normally. If a spatial server has exited, the simulation in its spatial area stops, and cross-server will also fail.

## There is a conflict with other Protobuf libraries in the project
The ChanneldUE plugin uses the Protobuf library and references it in the form of the ProtobufUE module. If your project also uses the Protobuf library, you need to change the ChanneldUE plugin or the project's reference of ProtobufUE to your own module, and then recompile. Note that the include path used by ChannelUE is `google/protobuf/*.h`. If the include path of your Protobuf library is different, ChanneldUE will have compilation errors.

## The project has an asset "Empty Engine Version" warning
If the following warning message appears in the project:
```log
LogLinker: Warning: Asset 'Your_Project_Root/Plugins/channeld-ue-plugin/Content/Blueprints/BP_SpatialRegionBox.uasset' has been saved with empty engine version. The asset will be loaded but may be incompatible.
```
Please add the following content to `Config/DefaultEngine.ini` of the project:
```ini
[Core.System]
ZeroEngineVersionWarning=False
```
