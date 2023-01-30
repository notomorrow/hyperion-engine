<%def name="do_fn(num_args)">
    <% arg_template_str = ', '.join(["class Arg{}Type".format(i) for i in range(0, num_args)]) %>

    template <class ReturnType${", {}".format(arg_template_str) if len(arg_template_str) else ""}, ReturnType(*Fn)(${', '.join(["Arg{}Type".format(i) for i in range(0, num_args)])})>
    HYP_SCRIPT_FUNCTION(CxxFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args});

        using Normalized = NormalizedType<ReturnType>;

        <% invoke_args = [] %>

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int32_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT32(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<int64_t, Normalized>) {
            HYP_SCRIPT_RETURN_INT64(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint32_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT32(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<uint64_t, Normalized>) {
            HYP_SCRIPT_RETURN_UINT64(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<float, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT32(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<double, Normalized>) {
            HYP_SCRIPT_RETURN_FLOAT64(Fn(${', '.join(invoke_args)}));
        } else if constexpr (std::is_same_v<bool, Normalized>) {
            HYP_SCRIPT_RETURN_BOOLEAN(Fn(${', '.join(invoke_args)}));
        } else {
            HYP_SCRIPT_CREATE_PTR(Fn(${', '.join(invoke_args)}), result);

            const auto class_name_it = params.api_instance.class_bindings.class_names.Find<Normalized>();
            AssertThrowMsg(class_name_it != params.api_instance.class_bindings.class_names.End(), "Class not registered!");

            const auto prototype_it = params.api_instance.class_bindings.class_prototypes.find(class_name_it->second);
            AssertThrowMsg(prototype_it != params.api_instance.class_bindings.class_prototypes.end(), "Class not registered!");

            vm::VMObject result_value(prototype_it->second); // construct from prototype
            HYP_SCRIPT_SET_MEMBER(result_value, "__intern", result);

            HYP_SCRIPT_CREATE_PTR(result_value, ptr);

            HYP_SCRIPT_RETURN(ptr);
        }
    }
</%def>

${do_fn(0)}
${do_fn(1)}
${do_fn(2)}
${do_fn(3)}
${do_fn(4)}
${do_fn(5)}
${do_fn(6)}
${do_fn(7)}
${do_fn(8)}
${do_fn(9)}
${do_fn(10)}