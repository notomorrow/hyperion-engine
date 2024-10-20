#pragma region ${hyp_class.name}

<% is_component = hyp_class.has_attribute("component") %> \
% if is_component:
${"#include <scene/ecs/ComponentInterface.hpp>"}
% endif

namespace hyperion {

<% start_macro_names = { HypClassType.CLASS: 'HYP_BEGIN_CLASS', HypClassType.STRUCT: 'HYP_BEGIN_STRUCT' } %> \
<% end_macro_names = { HypClassType.CLASS: 'HYP_END_CLASS', HypClassType.STRUCT: 'HYP_END_STRUCT' } %> \

<% class_attributes = ','.join([f"HypClassAttribute(\"{name.lower()}\", {format_attribute_value(value, attr_type)})" for name, value, attr_type in hyp_class.attributes]) %> \
\
${start_macro_names[hyp_class.class_type]}(${hyp_class.name}${(f", NAME(\"{hyp_class.base_class.name}\")" if hyp_class.base_class else ", {}") if hyp_class.class_type == HypClassType.CLASS else ''}${(', ' + class_attributes) if class_attributes else ''}) \
    <% s = "" %> \
    % for i in range(0, len(hyp_class.members)):
        <% member = hyp_class.members[i] %> \
        <% attributes = ("Span<HypClassAttribute> { { " + ', '.join([f"HypClassAttribute(\"{name.lower()}\", {format_attribute_value(value, attr_type)})" for name, value, attr_type in member.attributes]) + " } }") if len(member.attributes) > 0 else '' %> \
        % if member.member_type == HypMemberType.FIELD:
            <% s += f"HypField(NAME(HYP_STR({member.name})), &Type::{member.name}, offsetof(Type, {member.name}){', ' + attributes if len(member.attributes) > 0 else ''})" %> \
        % elif member.member_type == HypMemberType.METHOD:
            <% s += f"HypMethod(NAME(HYP_STR({member.name})), &Type::{member.name}{', ' + attributes if len(member.attributes) > 0 else ''})" %> \
        % elif member.member_type == HypMemberType.PROPERTY:
            <% property_args = ', '.join(member.args) %> \
            <% s += f"HypProperty(NAME(HYP_STR({member.name})), {property_args})" %> \
        % endif
        <% s += ',' if i != len(hyp_class.members) - 1 else '' %> \
        <% s += '\n' %> \
    % endfor
    ${s}
${end_macro_names[hyp_class.class_type]}

% if is_component:
    ${f"HYP_REGISTER_COMPONENT({hyp_class.name});"}
% endif

} // namespace hyperion

#pragma endregion ${hyp_class.name}

