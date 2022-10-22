#include <builders/shader_compiler/ShaderCompiler.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/ShaderBundleMarshal.hpp>
#include <math/MathUtil.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

static const FlatMap<String, ShaderModule::Type> shader_type_names = {
    { "vert", ShaderModule::Type::VERTEX },
    { "frag", ShaderModule::Type::FRAGMENT },
    { "geom", ShaderModule::Type::GEOMETRY },
    { "comp", ShaderModule::Type::COMPUTE },
    { "rgen", ShaderModule::Type::RAY_GEN },
    { "rchit", ShaderModule::Type::RAY_CLOSEST_HIT },
    { "rahit", ShaderModule::Type::RAY_ANY_HIT },
    { "rmiss", ShaderModule::Type::RAY_MISS },
    { "rint", ShaderModule::Type::RAY_INTERSECT },
    { "tesc", ShaderModule::Type::TESS_CONTROL },
    { "mesh", ShaderModule::Type::MESH },
    { "task", ShaderModule::Type::TASK }
};

static void ForEachPermutation(
    const DynArray<String> &props,
    Proc<void, const DynArray<String> &> callback
)
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

ShaderCompiler::ShaderCompiler(Engine *engine)
    : m_engine(engine),
      m_definitions(nullptr)
{
}

ShaderCompiler::~ShaderCompiler()
{
    if (m_definitions) {
        delete m_definitions;
    }
}

bool ShaderCompiler::ReloadCompiledShaderBatch(const String &name)
{
    if (!m_definitions || !m_definitions->IsValid()) {
        // load for first time if no definitions loaded
        LoadShaderDefinitions();

        CompiledShaderBatch tmp;

        return m_cache.Get(name, tmp);
    }

    if (!m_definitions->HasSection(name)) {
        // not in definitions file
        return false;
    }

    const auto &section = m_definitions->GetSection(name);

    Bundle bundle {
        .name = name
    };

    for (const auto &section_it : section) {
        if (section_it.first == "permute") {
            // set each property
            for (const auto &element : section_it.second.elements) {
                bundle.props.Set(element, true);
            }
        } else if (shader_type_names.Contains(section_it.first)) {
            bundle.sources[shader_type_names.At(section_it.first)] = SourceFile {
                FilePath("res/vkshaders") / section_it.second.GetValue()
            };
        } else {
            DebugLog(
                LogType::Warn,
                "Unknown property in shader definition file: %s\n",
                section_it.first.Data()
            );
        }
    }

    return CompileBundle(bundle);
}

void ShaderCompiler::LoadShaderDefinitions()
{
    if (m_definitions) {
        delete m_definitions;
    }

    m_definitions = new DefinitionsFile(FilePath("res/shaders.def"));

    if (!m_definitions->IsValid()) {
        DebugLog(
            LogType::Warn,
            "Failed to load shader definitions file!\n"
        );

        delete m_definitions;
        m_definitions = nullptr;

        return;
    }

    DynArray<Bundle> bundles;

    // create a bundle for each section.
    for (const auto &it : m_definitions->GetSections()) {
        const auto &key = it.first;
        auto &section = it.second;

        Bundle bundle {
            .name = key
        };

        for (const auto &section_it : section) {
            if (section_it.first == "permute") {
                // set each property
                for (const auto &element : section_it.second.elements) {
                    bundle.props.Set(element, true);
                }
            } else if (shader_type_names.Contains(section_it.first)) {
                bundle.sources[shader_type_names.At(section_it.first)] = SourceFile {
                    FilePath("res/vkshaders") / section_it.second.GetValue()
                };
            } else {
                DebugLog(
                    LogType::Warn,
                    "Unknown property in shader definition file: %s\n",
                    section_it.first.Data()
                );
            }
        }

        if (GetCompiledShader(bundle.name, bundle.props)) {
            // compiled shader already exists with props, no need to recompile.
            // will be set in cache.
            
            continue;
        }

        bundles.PushBack(std::move(bundle));
    }

    DynArray<bool> results;
    results.Resize(bundles.Size());

    bundles.ParallelForEach(
        m_engine->task_system,
        [this, &results](const Bundle &bundle, UInt index, UInt) {
            results[index] = CompileBundle(bundle);
        }
    );
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

bool ShaderCompiler::CompileBundle(const Bundle &bundle)
{
    // run with spirv-cross
    FileSystem::Mkdir("data/compiled_shaders/tmp");

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

    const SizeType num_permutations = 1ull << bundle.props.Size();
    SizeType num_compiled_permutations = 0;

    if (num_permutations >= 64) {
        // too many permutations
        DebugLog(
            LogType::Error,
            "Too many shader permutations for shader %s\n",
            bundle.name.Data()
        );

        return false;
    }

    // compile each..
    DynArray<String> props;
    props.Reserve(bundle.props.Size());

    for (auto &item : bundle.props) {
        props.PushBack(item);
    }

    CompiledShaderBatch compiled_shader_batch;

    DebugLog(
        LogType::Info,
        "Compiling shader bundle for shader %s (%llu variants)\n",
        bundle.name.Data(),
        num_permutations
    );

    // compile shader with each permutation of properties
    ForEachPermutation(props, [&loaded_source_files, &compiled_shader_batch, &num_compiled_permutations](const DynArray<String> &items) {
        // get hashcode of this set of properties
        HashCode props_hash_code = items.GetHashCode();

        CompiledShader compiled_shader {
            .props_hash = props_hash_code.Value()
        };

        bool any_files_compiled = false;

        for (const auto &item : loaded_source_files) {
            // check if a file exists w/ same hash
            
            const auto output_filepath = item.GetOutputFilepath(props_hash_code);

            if (output_filepath.Exists()) {
                // file exists and is older than the original source file; no need to build
                if (output_filepath.LastModifiedTimestamp() >= item.last_modified_timestamp) {
                    // DebugLog(
                    //     LogType::Info,
                    //     "File %s exists and was modified later than the source file, not rebuilding...\n",
                    //     output_filepath.Data()
                    // );

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

            String permutation_string;

            for (SizeType index = 0; index < items.Size(); index++) {
                permutation_string += items[index];

                if (index != items.Size() - 1) {
                    permutation_string += ", ";
                }
            }

            DebugLog(
                LogType::Info,
                "Compiling shader %s with permutation [%s]...\n",
                output_filepath.Data(),
                permutation_string.Data()
            );

            if (system((String("glslc --target-env=vulkan1.1 ") + item.file.path + " -o " + output_filepath).Data())) {
                DebugLog(LogType::Error, "Failed to compile file %s with props hash %u!\n!", item.file.path.Data(), props_hash_code.Value());

                return;
            }

            any_files_compiled = true;

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

        num_compiled_permutations += static_cast<UInt>(any_files_compiled);
        compiled_shader_batch.compiled_shaders.PushBack(std::move(compiled_shader));
    });

    const FilePath final_output_path = FilePath("data/compiled_shaders") / bundle.name + ".hypshader";

    FileByteWriter byte_writer(final_output_path.Data());

    fbom::FBOMWriter writer;
    writer.Append(compiled_shader_batch);
    auto err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (err.value != fbom::FBOMResult::FBOM_OK) {
        return false;
    }

    m_cache.Set(bundle.name, std::move(compiled_shader_batch));

    DebugLog(
        LogType::Info,
        "Compiled %llu new variants for shader %s to: %s\n",
        num_compiled_permutations,
        bundle.name.Data(),
        final_output_path.Data()
    );

    return true;
}

CompiledShader ShaderCompiler::GetCompiledShader(
    const String &name
)
{
    return GetCompiledShader(name, ShaderProps { });
}

CompiledShader ShaderCompiler::GetCompiledShader(
    const String &name,
    const ShaderProps &props
)
{
    CompiledShader result;

    if (m_definitions && m_definitions->IsValid()) {
        if (!ReloadCompiledShaderBatch(name)) {
            DebugLog(
                LogType::Error,
                "Failed to attempt reloading of shader batch\n"
            );

            return result;
        }
    } else {
        DebugLog(
            LogType::Warn,
            "Shader definitions file not loaded! Cannot compare timestamps, shaders may be or become out of date...\n"
        );
    }

    const auto output_file_path = FilePath("data/compiled_shaders") / name + ".hypshader";

    const auto props_hash_code = props.GetHashCode();
    
    if (m_cache.GetShaderInstance(name, props_hash_code.Value(), result)) {
        return result;
    }

    fbom::FBOMReader reader(m_engine, fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject deserialized;

    DebugLog(
        LogType::Info,
        "Loading compiled shader %s...\n",
        output_file_path.Data()
    );

    if (auto err = reader.LoadFromFile(output_file_path, deserialized)) {
        return result;
    }
    
    if (auto compiled_shader_batch = deserialized.Get<CompiledShaderBatch>()) {
        // set in cache so we can use it later
        m_cache.Set(name, *compiled_shader_batch);

        auto it = compiled_shader_batch->compiled_shaders.FindIf([props_hash_code](const CompiledShader &compiled_shader) {
            return compiled_shader.props_hash == props_hash_code.Value();
        });

        if (it != compiled_shader_batch->compiled_shaders.End()) {
            result = *it;
        }
    }

    return result;
}

} // namespace hyperion::v2