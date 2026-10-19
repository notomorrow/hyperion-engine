include_guard(GLOBAL)

set(HYP_GENERATORS_PROJECT_DIR "${CSHARP_PROJECTS_DIR}/Hyperion.NET.Generators" CACHE INTERNAL "")
set(HYP_GENERATORS_CSPROJ_PATH "${HYP_GENERATORS_PROJECT_DIR}/Hyperion.NET.Generators.csproj" CACHE INTERNAL "")
set(HYP_GENERATORS_DLL_PATH "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Hyperion.NET.Generators.dll" CACHE INTERNAL "")

file(GLOB HYP_GENERATORS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cs")

file(MAKE_DIRECTORY ${HYP_GENERATORS_PROJECT_DIR})

file(WRITE ${HYP_GENERATORS_CSPROJ_PATH} "<Project Sdk=\"Microsoft.NET.Sdk\">\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  <PropertyGroup>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <Version>${HYP_VERSION_MAJOR}.${HYP_VERSION_MINOR}.${HYP_VERSION_PATCH}.0</Version>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <OutputType>Library</OutputType>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <TargetFramework>netstandard2.0</TargetFramework>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <LangVersion>latest</LangVersion>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <Nullable>enable</Nullable>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <AssemblyName>Hyperion.NET.Generators</AssemblyName>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <IsRoslynComponent>true</IsRoslynComponent>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <EnforceExtendedAnalyzerRules>true</EnforceExtendedAnalyzerRules>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <Platforms>AnyCPU;x64;ARM64</Platforms>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <PlatformTarget>AnyCPU</PlatformTarget>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <NoWarn>$(NoWarn);RS2008</NoWarn>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  </PropertyGroup>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  <ItemGroup>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <PackageReference Include=\"Microsoft.CodeAnalysis.CSharp\" Version=\"4.9.2\" PrivateAssets=\"all\" />\n")
foreach(HYP_GENERATORS_SOURCE ${HYP_GENERATORS_SOURCES})
  file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <Compile Include=\"${HYP_GENERATORS_SOURCE}\" />\n")
endforeach()
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  </ItemGroup>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  <Target Name=\"CopyDLL\" AfterTargets=\"AfterBuild\">\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "    <Copy SourceFiles=\"$(TargetPath)\" DestinationFolder=\"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/\" />\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "  </Target>\n")
file(APPEND ${HYP_GENERATORS_CSPROJ_PATH} "</Project>\n")

find_program(DOTNET_EXE dotnet)

set(MSBuildSdksPath_BACKUP $ENV{MSBuildSdksPath})
unset(ENV{MSBuildSdksPath})

execute_process(
    COMMAND ${DOTNET_EXE} restore
    WORKING_DIRECTORY ${HYP_GENERATORS_PROJECT_DIR}
)

if(MSBuildSdksPath_BACKUP)
  set(ENV{MSBuildSdksPath} ${MSBuildSdksPath_BACKUP})
endif()

if(NOT USE_MSBUILD_FOR_CSHARP_BUILD)
  if(CMAKE_HOST_WIN32)
    set(HYP_GENERATORS_BUILD_WRAPPER "${HYP_GENERATORS_PROJECT_DIR}/dotnet-build.bat")
    file(WRITE ${HYP_GENERATORS_BUILD_WRAPPER} "@echo off\n")
    file(APPEND ${HYP_GENERATORS_BUILD_WRAPPER} "set MSBuildSdksPath=\n")
    file(APPEND ${HYP_GENERATORS_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
  else()
    set(HYP_GENERATORS_BUILD_WRAPPER "${HYP_GENERATORS_PROJECT_DIR}/dotnet-build.sh")
    file(WRITE ${HYP_GENERATORS_BUILD_WRAPPER} "#!/bin/bash\n")
    file(APPEND ${HYP_GENERATORS_BUILD_WRAPPER} "unset MSBuildSdksPath\n")
    file(APPEND ${HYP_GENERATORS_BUILD_WRAPPER} "\"${DOTNET_EXE}\" build --disable-build-servers --no-restore --configuration ${HYP_BUILD_TYPE}\n")
    execute_process(COMMAND chmod +x ${HYP_GENERATORS_BUILD_WRAPPER})
  endif()

  add_custom_target(Hyperion.NET.Generators ALL
      COMMAND ${HYP_GENERATORS_BUILD_WRAPPER}
      WORKING_DIRECTORY ${HYP_GENERATORS_PROJECT_DIR}
  )
endif()

function(hyp_append_type_registry_generator CSPROJ_PATH)
  if(USE_MSBUILD_FOR_CSHARP_BUILD)
    file(APPEND ${CSPROJ_PATH} "    <ProjectReference Include=\"${HYP_GENERATORS_CSPROJ_PATH}\" OutputItemType=\"Analyzer\" ReferenceOutputAssembly=\"false\" />\n")
  else()
    file(APPEND ${CSPROJ_PATH} "    <Analyzer Include=\"${HYP_GENERATORS_DLL_PATH}\" />\n")
  endif()
endfunction()
