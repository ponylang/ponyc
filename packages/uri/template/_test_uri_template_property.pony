use "pony_test"
use "pony_check"

class \nodoc\ iso _TestPropertyNoBracesInExpansion is Property1[String]
  """Property: expansion output contains no raw braces."""
  fun name(): String => "uri/template/property: no braces in expansion"

  fun gen(): Generator[String] =>
    // Generate valid templates from building blocks
    _TemplateGenerators.valid_template()

  fun ref property(arg1: String, h: PropertyHelper) =>
    match \exhaustive\ URITemplateParse(arg1)
    | let tpl: URITemplate =>
      let vars = _RFC6570Vars()
      let result: String val = tpl.expand(vars)
      for byte in result.values() do
        if byte == '{' then
          h.fail("expansion contains '{' for template: " + arg1)
          return
        end
        if byte == '}' then
          h.fail("expansion contains '}' for template: " + arg1)
          return
        end
      end
    | let err: URITemplateParseError =>
      h.fail("generated template should be valid: " + arg1 +
        " error: " + err.string())
    end

class \nodoc\ iso _TestPropertyUnreservedPassthrough is Property1[String]
  """Property: unreserved values in simple expansion pass through unchanged."""
  fun name(): String =>
    "uri/template/property: unreserved value passthrough"

  fun gen(): Generator[String] =>
    _TemplateGenerators.unreserved_string(1, 30)

  fun ref property(arg1: String, h: PropertyHelper) =>
    let vars = URITemplateVariables
    vars.set("x", arg1)
    try
      let tpl = URITemplate("{x}")?
      let result: String val = tpl.expand(vars)
      h.assert_eq[String val](arg1, result)
    else
      h.fail("failed to parse {x}")
    end

class \nodoc\ iso _TestPropertyValidTemplatesParse is Property1[String]
  """Property: valid generated templates always parse successfully."""
  fun name(): String => "uri/template/property: valid templates parse"

  fun gen(): Generator[String] =>
    _TemplateGenerators.valid_template()

  fun ref property(arg1: String, h: PropertyHelper) =>
    match \exhaustive\ URITemplateParse(arg1)
    | let _: URITemplate => None
    | let err: URITemplateParseError =>
      h.fail("valid template failed to parse: '" + arg1 +
        "' error: " + err.string())
    end

class \nodoc\ iso _TestPropertyInvalidTemplatesFail is Property1[String]
  """Property: invalid generated templates always fail to parse."""
  fun name(): String => "uri/template/property: invalid templates fail"

  fun gen(): Generator[String] =>
    _TemplateGenerators.invalid_template()

  fun ref property(arg1: String, h: PropertyHelper) =>
    match \exhaustive\ URITemplateParse(arg1)
    | let _: URITemplate =>
      h.fail("invalid template should not parse: '" + arg1 + "'")
    | let _: URITemplateParseError => None
    end

class \nodoc\ iso _TestPropertyMixedTemplates is Property1[(String, Bool)]
  """
  Property: mixed valid/invalid templates succeed iff they're the valid variant.
  """
  fun name(): String => "uri/template/property: mixed valid/invalid boundary"

  fun gen(): Generator[(String, Bool)] =>
    _TemplateGenerators.mixed_template()

  fun ref property(arg1: (String, Bool), h: PropertyHelper) =>
    (let template, let is_valid) = arg1
    match \exhaustive\ URITemplateParse(template)
    | let _: URITemplate =>
      if not is_valid then
        h.fail("invalid template should not parse: '" + template + "'")
      end
    | let err: URITemplateParseError =>
      if is_valid then
        h.fail("valid template failed to parse: '" + template +
          "' error: " + err.string())
      end
    end

primitive \nodoc\ _TemplateGenerators
  """
  Generators for property-based URI template tests.
  """
  fun unreserved_string(
    min: USize = 0,
    max: USize = 30)
    : Generator[String]
  =>
    let unreserved_bytes: Array[U8] val =
      recover val
        let arr = Array[U8]
        let chars =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
        for ch in chars.values() do
          arr.push(ch)
        end
        arr
      end
    Generators.byte_string(
      Generators.usize(0, unreserved_bytes.size() - 1)
        .map[U8]({(idx) =>
          try unreserved_bytes(idx)? else 'a' end
        }),
      min,
      max)

  fun valid_varname(): Generator[String] =>
    let name_bytes: Array[U8] val =
      recover val
        let arr = Array[U8]
        let chars =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"
        for ch in chars.values() do
          arr.push(ch)
        end
        arr
      end
    Generators.byte_string(
      Generators.usize(0, name_bytes.size() - 1)
        .map[U8]({(idx) =>
          try name_bytes(idx)? else 'a' end
        }),
      1,
      10)

  fun valid_expression(): Generator[String] =>
    let ops: Array[String val] val = [""; "+"; "#"; "."; "/"; ";"; "?"; "&"]
    Generators.usize(0, ops.size() - 1)
      .flat_map[String]({(op_idx)(ops) =>
        _TemplateGenerators.valid_varname()
          .map[String]({(name)(op_idx, ops) =>
            let op = try ops(op_idx)? else "" end
            recover val
              String .> append("{")
                .> append(op)
                .> append(name)
                .> append("}")
            end
          })
      })

  fun valid_template(): Generator[String] =>
    // Generate 1-3 parts (mix of literals and expressions)
    Generators.usize(1, 3)
      .flat_map[String]({(num_parts) =>
        // Build a template by alternating literals and expressions
        _TemplateGenerators.valid_expression()
          .map[String]({(expr)(num_parts) =>
            if num_parts > 1 then
              recover val
                String .> append("http://example.com/")
                  .> append(expr)
              end
            else
              expr
            end
          })
      })

  fun invalid_template(): Generator[String] =>
    // Generate various kinds of invalid templates
    Generators.frequency[String](
      [
          // Unclosed expression
        (1, _TemplateGenerators.valid_varname()
          .map[String]({(name) =>
            recover val String .> append("{") .> append(name) end
          }))
        // Reserved operator
        (1, _TemplateGenerators.valid_varname()
          .map[String]({(name) =>
            recover val
              String .> append("{=") .> append(name) .> append("}")
            end
          }))
        // Empty expression
        (1, Generators.unit[String]("{}"))
        // Stray close brace
        (1, Generators.unit[String]("foo}bar"))
        // Space in literal
        (1, Generators.unit[String]("foo bar"))
      ])

  fun mixed_template(): Generator[(String, Bool)] =>
    Generators.bool()
      .flat_map[(String, Bool)]({(is_valid) =>
        if is_valid then
          _TemplateGenerators.valid_template()
            .map[(String, Bool)]({(tpl) => (tpl, true) })
        else
          _TemplateGenerators.invalid_template()
            .map[(String, Bool)]({(tpl) => (tpl, false) })
        end
      })
