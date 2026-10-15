/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Editor/EditorImport.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/Tasks/EditorTasks.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

void ImportEditorContent(EditorSubsystem* subsystem, const Array<FilePath>& files, EditorImportCallback&& onImported)
{
    if (files.Empty())
    {
        return;
    }

    struct ImportProgressContext
    {
        AtomicVar<uint32> numProcessed { 0 };
        AtomicVar<uint32> numFailed { 0 };
        uint32 numTotal = 0;
        EditorTaskBase* task = nullptr;
        EditorImportCallback onImported;
    };

    auto context = MakeShared<ImportProgressContext>();
    context->numTotal = uint32(files.Size());
    context->onImported = std::move(onImported);

    EditorTaskScope* editorTaskScope = new EditorTaskScope(
        TickableEditorTask::StaticClass(),
        [context]()
        {
            const uint32 numProcessed = context->numProcessed.Get(MemoryOrder::RELAXED);
            const uint32 numFailed = context->numFailed.Get(MemoryOrder::RELAXED);

            if (EditorTaskBase* task = context->task)
            {
                task->SetProgress(context->numTotal > 0
                        ? MathUtil::Min(float(numProcessed) / float(context->numTotal), 1.0f)
                        : 1.0f);

                task->SetDescription(numFailed > 0
                        ? HYP_FORMAT("Importing {}/{} assets ({} failed)", numProcessed, context->numTotal, numFailed)
                        : HYP_FORMAT("Importing {}/{} assets", numProcessed, context->numTotal));
            }
        },
        "Importing content...",
        HYP_FORMAT("Importing 0/{} assets", context->numTotal),
        /* isForegroundTask */ true);

    context->task = editorTaskScope->GetEditorTask();

    // Create identifier based on the common folder of the assets
    const String identifier = files[0].BasePath().Basename();

    AssetBatch* batch = AssetManager::GetInstance()->CreateBatch(identifier);

    for (const FilePath& file : files)
    {
        batch->Add(file.Basename(), file, GetEditorImportLoadHint(file));
    }

    batch->GetCallbacks().OnItemComplete
        .Bind([context](const AssetBatchCallbackData&)
              {
                  context->numProcessed.Increment(1, MemoryOrder::RELEASE);
              })
        .Detach();

    batch->GetCallbacks().OnItemFailed
        .Bind([context](const AssetBatchCallbackData&)
              {
                  context->numFailed.Increment(1, MemoryOrder::RELEASE);
                  context->numProcessed.Increment(1, MemoryOrder::RELEASE);
              })
        .Detach();

    batch->OnComplete
        .Bind([context, editorTaskScope, weakSubsystem = MakeWeakRef(subsystem)](AssetMap& results) mutable
              {
                  HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                  String postText;
                  int numFailed = 0;
                  uint32 numProcessed = 0;

                  Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();

                  Array<Handle<AssetObject>> importedAssets;
                  Set<uint32, EditorAllocator> changedBuckets;

                  for (auto& it : results)
                  {
                      String& key = it.first;
                      LoadedAsset& loadedAsset = it.second;

                      editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", ++numProcessed, results.Size(), key) + postText);

                      if (!loadedAsset.IsValid())
                      {
                          HYP_LOG(Editor, Error, "Failed to import asset '{}': {}", key, loadedAsset.GetError().GetMessage());

                          postText = HYP_FORMAT(" ({} failed)", ++numFailed);

                          editorTaskScope->GetEditorTask()->SetDescription(HYP_FORMAT("Processing {}/{}: {}", numProcessed, results.Size(), key) + postText);

                          continue;
                      }

                      Handle<AssetObject> assetObject = loadedAsset.ExtractAs<AssetObject>();
                      if (!assetObject.IsValid())
                      {
                          continue;
                      }

                      GetCurrentAssetRegistry()->PutAssetUnique(assetObject);

                      importedAssets.PushBack(assetObject);

                      const AssetPath& assetPath = assetObject->GetPath();

                      if (assetPath.IsValid() && subsystem.IsValid())
                      {
                          changedBuckets.Insert(assetPath.GetBucket().GetIndex());
                      }
                  }

                  for (uint32 bucketIndex : changedBuckets)
                  {
                      subsystem->OnAssetsChanged(bucketIndex);
                  }

                  delete editorTaskScope;

                  if (subsystem.IsValid() && context->onImported.IsValid())
                  {
                      context->onImported(*subsystem, importedAssets);
                  }
              })
        .Detach();

    batch->LoadAsync();

    // Note: The batch will be destroyed automatically by AssetManager when complete
}

} // namespace Hyperion
