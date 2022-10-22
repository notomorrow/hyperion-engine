#include <builders/shader_compiler/ShaderCompiler.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/ShaderBundleMarshal.hpp>
#include <math/MathUtil.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

void ForEachPermutation(const DynArray<String> &props, Proc<void, const DynArray<String> &> callback)
{
    SizeType num_permutations = 1ull << props.Size();

    for (SizeType i = 0; i < num_permutations; i++) {
        DynArray<String> tmp_props;
        tmp_props.Reserve(MathUtil::BitCount(i));

        for (SizeType j = 0; j < props.Size(); j++) {
            if (i & (1 << j)) {
                tmp_props.PushBack(props[j]);
            }
        }

        callback(tmp_props);
    }
}

void ShaderCompiler::CompileBundles()
{
    for (auto &bundle : m_bundles) {
        AssertThrow(CompileBundle(bundle));
    }
}

struct LoadedSourceFile
{
    ShaderModule::Type type;
    ShaderCompiler::SourceFile file;
    UInt64 last_modified_timestamp;
    String original_source;

    FilePath GetOutputFilepath(HashCode props_hash_code) const
    {
        return FilePath("data/compiled_shaders/tmp/") + FilePath(file.path).Basename()
            + "_" + String::ToString(props_hash_code.Value()) + ".shc";
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(file);
        hc.Add(last_modified_timestamp);
        hc.Add(original_source);

        return hc;
    }
};

bool ShaderCompiler::CompileBundle(Bundle &bundle)
{
    // run with spirv-cross
    FileSystem::Mkdir("data/compiled_shaders/tmp");

    struct CompiledSourceFile
    {
        FilePath path;
    };

    struct CompiledShaderBundle
    {
        FilePath metadata_path;
        DynArray<CompiledSourceFile> compiled_files;

        // void SaveMetadata() const
        // {
        //     FileByteWriter fbw;
        // }
        
        // UInt64 GetShaderPropsHashCode() const
        // {
        //     auto basename = 
        // }
    };


    DynArray<LoadedSourceFile> loaded_source_files;
    loaded_source_files.Reserve(bundle.sources.Size());

    for (auto &it : bundle.sources) {
        const auto filepath = FilePath(it.second.path);
        auto source_stream = filepath.Open();

        if (!source_stream.IsOpen()) {
            // could not open file!
            return false;
        }

        ByteBuffer byte_buffer = source_stream.ReadBytes();

        loaded_source_files.PushBack(LoadedSourceFile {
            it.first,
            it.second,
            filepath.LastModifiedTimestamp(),
            String(byte_buffer)
        });
    }

    SizeType num_permutations = 1ull << bundle.props.Size();

    if (num_permutations >= 64) {
        // too many permutations
        return false;
    }

    // compile each..
    DynArray<String> props;
    props.Reserve(bundle.props.Size());

    for (auto &item : bundle.props) {
        props.PushBack(item);
    }

    DebugLog(
        LogType::Info,
        "Compiling shader bundle (%llu variants)\n",
        num_permutations
    );

    CompiledShaderBatch compiled_shader_batch;

    // compile shader with each permutation of properties
    ForEachPermutation(props, [&loaded_source_files, &compiled_shader_batch](const DynArray<String> &items) {
        // get hashcode of this set of properties
        HashCode props_hash_code = items.GetHashCode();

        CompiledShader compiled_shader {
            .props_hash = props_hash_code.Value()
        };

        // compile shader with set of properties

        for (const auto &item : loaded_source_files) {
            // check if a file exists w/ same hash
            
            const auto output_filepath = item.GetOutputFilepath(props_hash_code);

            if (output_filepath.Exists()) {
                // file exists and is older than the original source file; no need to build
                if (output_filepath.LastModifiedTimestamp() >= item.last_modified_timestamp) {
                    DebugLog(
                        LogType::Info,
                        "File %s exists and was modified later than the source file, not rebuilding...\n",
                        output_filepath.Data()
                    );

                    if (auto stream = output_filepath.Open()) {
                        compiled_shader.modules[item.type] = stream.ReadBytes();

                        continue;
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "File %s seems valid for reuse but could not be opened. Attempting to rebuild...\n",
                            output_filepath.Data()
                        );
                    }
                }
            }

            if (system((String("glslc --target-env=vulkan1.1 ") + item.file.path + " -o " + output_filepath).Data())) {
                DebugLog(LogType::Error, "Failed to compile file %s with props hash %u!\n!", item.file.path.Data(), props_hash_code.Value());

                return;
            }

            if (auto stream = output_filepath.Open()) {
                compiled_shader.modules[item.type] = stream.ReadBytes();
            } else {
                DebugLog(
                    LogType::Error,
                    "Failed to open compiled shader binary at %s!\n",
                    output_filepath.Data()
                );

                return;
            }
        }

        compiled_shader_batch.compiled_shaders.PushBack(std::move(compiled_shader));
    });

    FileByteWriter byte_writer((FilePath("data/compiled_shaders") / bundle.name + ".fbom").Data());

    fbom::FBOMWriter writer;
    writer.Append(compiled_shader_batch);
    auto err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (err.value != fbom::FBOMResult::FBOM_OK) {
        return false;
    }

    return true;
}

bool ShaderCompiler::GetCompiledShader(
    Engine *engine,
    const String &name,
    const ShaderProps &props,
    CompiledShader &out
)
{
    const auto props_hash_code = props.GetHashCode();

    // it.second is a basename-only key
    const auto output_file_path = FilePath("data/compiled_shaders") / name + ".fbom";

    fbom::FBOMReader reader(engine, fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject result;

    DebugLog(
        LogType::Info,
        "Loading compiled shader %s...\n",
        output_file_path.Data()
    );

    if (auto err = reader.LoadFromFile(output_file_path, result)) {
        return false;
    }
    
    if (auto compiled_shader_batch = result.Get<CompiledShaderBatch>()) {
        auto it = compiled_shader_batch->compiled_shaders.FindIf([props_hash_code](const CompiledShader &compiled_shader) {
            return compiled_shader.props_hash == props_hash_code.Value();
        });

        if (it == compiled_shader_batch->compiled_shaders.End()) {
            return false;
        }

        out = *it;
    } else {
        return false;
    }

    return true;
}

} // namespace hyperion::v2