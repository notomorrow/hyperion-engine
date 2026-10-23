/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

struct BoxedValue;
class Class;
class IMember;

namespace utilities {
struct TypeInfo;
struct TypeId;
} // namespace utilities

using utilities::TypeId;
using utilities::TypeInfo;

namespace DataProcessing::JSON {

class Value;
class Object;

} // namespace DataProcessing::JSON

namespace JSON = DataProcessing::JSON;

namespace functional {

template <class FunctionSignature>
class ProcRef;

} // namespace functional

using functional::ProcRef;

#pragma region JSON

struct ToJSONOptions
{
    enum class FollowAssetPathsMode : int
    {
        Never = -1,
        MatchingAttribute = 0, //!< with "FollowAssetPath" attribute evaluating to true
        Always
    };

    enum class SaveAssetsAsReferencesMode : int
    {
        No,
        UnlessOtherwiseSpecified,
        Yes
    };

    bool skipTransientProperties = true;
    bool writeClassNames = false;
    bool writeClassNamesRecursively = false;

    SaveAssetsAsReferencesMode saveAssetsAsReferences = SaveAssetsAsReferencesMode::UnlessOtherwiseSpecified;
    FollowAssetPathsMode followAssetPaths = FollowAssetPathsMode::MatchingAttribute;
};

/*! \brief Converts a BoxedValue to a Value.
 *
 *  \param value The BoxedValue to convert.
 *  \param outJson The output Value.
 *  \return True if conversion was successful, false otherwise.
 */
ENGINE_API Result BoxedToJSON(
    const BoxedValue& value,
    JSON::Value& outJson,
    ToJSONOptions* pOptions = nullptr);

/*! \brief Serializes a Object instance to a Object.
 *  Only fields and properties of the Class are serialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members marked with the "transient" attribute are skipped if skipTransientProperties is true.
 *  Members with the "jsonpath" attribute are serialized using the specified JSON path.
 *  \param cls The Class of the object.
 *  \param target The BoxedValue object to serialize.
 *  \param outJson The output Object.
 *  \return True if serialization was successful, false otherwise.
 */
ENGINE_API Result ObjectToJSON(
    const Class* cls,
    const BoxedValue& target,
    JSON::Object& outJson,
    ToJSONOptions* pOptions = nullptr);

/*! \brief Converts a JSON value to BoxedValue
 *
 *  \param jsonValue The Value to convert.
 *  \param typeInfo The TypeInfo for the target type.
 *  \param outBoxed The output BoxedValue.
 *  \return True if conversion was successful, false otherwise.
 */
ENGINE_API Result BoxedFromJSON(const JSON::Value& jsonValue, const TypeInfo& typeInfo, BoxedValue& outBoxed);

/*! \brief Deserializes a Object to a BoxedValue object.
 *  Only fields and properties of the Class are deserialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members with the "jsonpath" attribute are deserialized using the specified JSON path.
 *  \param jsonObject The Object to deserialize.
 *  \param targetClass The desired Class of the target object. (can be null if target already has a type)
 *  \param target The output BoxedValue object.
 *  \return True if deserialization was successful, false otherwise.
 */
ENGINE_API Result ObjectFromJSON(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target);

#pragma endregion JSON

#pragma region Hyperion Manifest

enum class CopyMembersFlags : uint32
{
    None = 0x0,
    IncludeTransient = 0x1
};

HYP_MAKE_ENUM_FLAGS(CopyMembersFlags);

struct ToHMFOptions
{
    enum class FollowAssetPathsMode : int
    {
        Never = -1,
        MatchingAttribute = 0,
        Always
    };

    enum class SaveAssetsAsReferencesMode : int
    {
        No,
        UnlessOtherwiseSpecified,
        Yes
    };

    bool skipTransientProperties = true;
    bool writeClassName = true;
    bool writeClassNamesForPolymorphic = true;

    SaveAssetsAsReferencesMode saveAssetsAsReferences = SaveAssetsAsReferencesMode::UnlessOtherwiseSpecified;
    FollowAssetPathsMode followAssetPaths = FollowAssetPathsMode::MatchingAttribute;
};
/*! \brief A handle member CopyMembersByName couldn't set.
 *  its value is an object of another definition of the member's type,
 *  so it can only be set once that object's replacement exists. */
struct DeferredHandleCopy
{
    BoxedValue target;
    const IMember* member = nullptr;
    Handle<ObjectBase> value;
};

struct DeferredHandleCopiesContext
{
    Array<DeferredHandleCopy>* deferredCopies = nullptr;
};

ENGINE_API Result BoxedToHMF(
    const BoxedValue& value,
    String& outText,
    const TypeInfo* declaredTypeInfo = nullptr,
    ToHMFOptions* pOptions = nullptr);

ENGINE_API Result ObjectToHMF(
    const Class* cls,
    const BoxedValue& target,
    String& outText,
    ToHMFOptions* pOptions = nullptr);

/*! \brief Reads the object in HMF from the given \p stream with only the given top-level fields set */
ENGINE_API TResult<BoxedValue> ObjectFieldsFromHMF(ByteReader& stream, Span<const StringHash> fieldNames);

/*! \brief Reads one top-level field of the object in a HMF, from \p stream */
ENGINE_API TResult<BoxedValue> FieldFromHMF(ByteReader& stream, StringHash fieldName);

ENGINE_API TResult<BoxedValue> BoxedAsType(const BoxedValue& value, const TypeInfo& typeInfo);

/*! \brief Copies the fields and properties of \p source onto \p target, matched by name.
 *  \param stopAtClass The first base class whose members aren't copied, or null to copy them all */
ENGINE_API void CopyMembersByName(
    const Class* sourceClass,
    const BoxedValue& source,
    const Class* targetClass,
    BoxedValue& target,
    const Class* stopAtClass = nullptr,
    EnumFlags<CopyMembersFlags> flags = CopyMembersFlags::None);


#pragma endregion Hyperion Manifest

#pragma region Shared

ENGINE_API void WalkBoxedValue(
    const BoxedValue& target,
    const ProcRef<void(const BoxedValue& current)>& func);

/*! \brief Resets all members marked with the Transient attribute to their default values.
 *  Used when cloning objects to prevent transient runtime state from being copied.
 *  \param value The BoxedValue to strip transient members from (modified in place). */
ENGINE_API void StripTransientMembers(BoxedValue& value);

/*! \brief Creates a clone of the source BoxedValue with all transient members reset to defaults.
 *  Creates a new default instance of the same type, then copies only non-transient members.
 *  \param src The source BoxedValue to clone.
 *  \param outDst The output BoxedValue (will be overwritten with the cleaned clone).
 *  \return True if the clone was successful. */
ENGINE_API bool CloneWithoutTransientMembers(
    const BoxedValue& src,
    BoxedValue& outDst);

#pragma endregion Shared

} // namespace Hyperion
