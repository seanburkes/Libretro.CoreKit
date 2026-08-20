namespace Libretro.Core.Abi;

public static unsafe class LibretroAbi
{
    public static bool IsSupportedLayout()
    {
        if (sizeof(nuint) != 8)
        {
            return false;
        }

        RetroSystemInfo systemInfo = default;
        RetroGameGeometry geometry = default;
        RetroSystemTiming timing = default;
        RetroSystemAvInfo avInfo = default;
        RetroGameInfo gameInfo = default;
        RetroInputDescriptor inputDescriptor = default;
        RetroVariable variable = default;
        RetroMessage message = default;
        RetroMessageExtended extendedMessage = default;
        RetroLogCallback logCallback = default;
        RetroCoreOptionV2Category optionCategory = default;
        RetroCoreOptionV2Definition optionDefinition = default;
        RetroCoreOptionsV2 options = default;
        RetroFrontendCallbacks callbacks = default;

        return sizeof(RetroSystemInfo) == 32 &&
            Offset(&systemInfo, &systemInfo.LibraryName) == 0 &&
            Offset(&systemInfo, &systemInfo.LibraryVersion) == 8 &&
            Offset(&systemInfo, &systemInfo.ValidExtensions) == 16 &&
            Offset(&systemInfo, &systemInfo.NeedFullPath) == 24 &&
            Offset(&systemInfo, &systemInfo.BlockExtract) == 25 &&
            sizeof(RetroGameGeometry) == 20 &&
            Offset(&geometry, &geometry.BaseWidth) == 0 &&
            Offset(&geometry, &geometry.BaseHeight) == 4 &&
            Offset(&geometry, &geometry.MaxWidth) == 8 &&
            Offset(&geometry, &geometry.MaxHeight) == 12 &&
            Offset(&geometry, &geometry.AspectRatio) == 16 &&
            sizeof(RetroSystemTiming) == 16 &&
            Offset(&timing, &timing.FramesPerSecond) == 0 &&
            Offset(&timing, &timing.SampleRate) == 8 &&
            sizeof(RetroSystemAvInfo) == 40 &&
            Offset(&avInfo, &avInfo.Geometry) == 0 &&
            Offset(&avInfo, &avInfo.Timing) == 24 &&
            sizeof(RetroGameInfo) == 32 &&
            Offset(&gameInfo, &gameInfo.Path) == 0 &&
            Offset(&gameInfo, &gameInfo.Data) == 8 &&
            Offset(&gameInfo, &gameInfo.Size) == 16 &&
            Offset(&gameInfo, &gameInfo.Metadata) == 24 &&
            sizeof(RetroInputDescriptor) == 24 &&
            Offset(&inputDescriptor, &inputDescriptor.Port) == 0 &&
            Offset(&inputDescriptor, &inputDescriptor.Device) == 4 &&
            Offset(&inputDescriptor, &inputDescriptor.Index) == 8 &&
            Offset(&inputDescriptor, &inputDescriptor.Id) == 12 &&
            Offset(&inputDescriptor, &inputDescriptor.Description) == 16 &&
            sizeof(RetroVariable) == 16 &&
            Offset(&variable, &variable.Key) == 0 &&
            Offset(&variable, &variable.Value) == 8 &&
            sizeof(RetroMessage) == 16 &&
            Offset(&message, &message.Text) == 0 &&
            Offset(&message, &message.Frames) == 8 &&
            sizeof(RetroMessageExtended) == 32 &&
            Offset(&extendedMessage, &extendedMessage.Text) == 0 &&
            Offset(&extendedMessage, &extendedMessage.DurationMilliseconds) == 8 &&
            Offset(&extendedMessage, &extendedMessage.Priority) == 12 &&
            Offset(&extendedMessage, &extendedMessage.Level) == 16 &&
            Offset(&extendedMessage, &extendedMessage.Target) == 20 &&
            Offset(&extendedMessage, &extendedMessage.Type) == 24 &&
            Offset(&extendedMessage, &extendedMessage.Progress) == 28 &&
            sizeof(RetroLogCallback) == 8 &&
            Offset(&logCallback, &logCallback.Log) == 0 &&
            sizeof(RetroCoreOptionValue) == 16 &&
            sizeof(RetroCoreOptionValues) ==
                LibretroConstants.CoreOptionValuesMaximum * sizeof(RetroCoreOptionValue) &&
            sizeof(RetroCoreOptionV2Category) == 24 &&
            Offset(&optionCategory, &optionCategory.Key) == 0 &&
            Offset(&optionCategory, &optionCategory.Description) == 8 &&
            Offset(&optionCategory, &optionCategory.Information) == 16 &&
            sizeof(RetroCoreOptionV2Definition) == 2104 &&
            Offset(&optionDefinition, &optionDefinition.Key) == 0 &&
            Offset(&optionDefinition, &optionDefinition.Description) == 8 &&
            Offset(&optionDefinition, &optionDefinition.CategorizedDescription) == 16 &&
            Offset(&optionDefinition, &optionDefinition.Information) == 24 &&
            Offset(&optionDefinition, &optionDefinition.CategorizedInformation) == 32 &&
            Offset(&optionDefinition, &optionDefinition.CategoryKey) == 40 &&
            Offset(&optionDefinition, &optionDefinition.Values) == 48 &&
            Offset(&optionDefinition, &optionDefinition.DefaultValue) == 2096 &&
            sizeof(RetroCoreOptionsV2) == 16 &&
            Offset(&options, &options.Categories) == 0 &&
            Offset(&options, &options.Definitions) == 8 &&
            sizeof(RetroFrontendCallbacks) == 48 &&
            Offset(&callbacks, &callbacks.Environment) == 0 &&
            Offset(&callbacks, &callbacks.VideoRefresh) == 8 &&
            Offset(&callbacks, &callbacks.AudioSample) == 16 &&
            Offset(&callbacks, &callbacks.AudioSampleBatch) == 24 &&
            Offset(&callbacks, &callbacks.InputPoll) == 32 &&
            Offset(&callbacks, &callbacks.InputState) == 40;
    }

    private static nuint Offset(void* value, void* field) =>
        (nuint)((byte*)field - (byte*)value);
}
