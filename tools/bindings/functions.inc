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
        } else {
            auto return_value = CxxToScriptValueInternal(params.api_instance, Fn(${', '.join(invoke_args)}));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
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