/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <generator/Generator.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <core/logging/Logger.hpp>

#include <core/io/ByteWriter.hpp>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

Result GeneratorBase::Generate(const Analyzer& analyzer, const Module& mod) const
{
    const FilePath outputFilePath = GetOutputFilePath(analyzer, mod);

    if (outputFilePath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Output file path is empty");
    }

    const FilePath basePath = outputFilePath.BasePath();

    if (!basePath.IsDirectory())
    {
        basePath.MkDir();
    }

    if (!basePath.IsDirectory())
    {
        HYP_LOG(BuildTool, Error, "Failed to create output directory: {}", basePath);

        return HYP_MAKE_ERROR(Error, "Failed to create output directory");
    }

    MemoryByteWriter memoryWriter;

    Result res = Generate(analyzer, mod, memoryWriter);

    if (!res.HasError())
    {
        FileByteWriter fileWriter { outputFilePath };
        fileWriter.Write(memoryWriter.GetBuffer());
    }

    return res;
}

} // namespace buildtool
} // namespace hyperion