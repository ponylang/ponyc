use ".."
use "pony_test"
use "collections"
use "peg"
use "itertools"
use "debug"

primitive JSONPathTests is TestList
  fun tag tests(test: PonyTest) =>
    test(_ParserRFCExamples)
    test(_ExamplesTest.create(_ChildSegmentRFCExamples))
    test(_ExamplesTest.create(_DescendantSegmentRFCExamples))
    test(_ExamplesTest.create(_MainRFCExamples))
    test(_ExamplesTest.create(_NullRFCExamples))
    test(_ExamplesTest.create(_ArraySliceRFCExamples))

class \nodoc\ iso _ParserRFCExamples is UnitTest
  let examples: Array[String] = [
    "$"
    "$['store']['book'][0]['title']"
    "$.store.book[0].title"
    "$.store.book[*].author"
    "$..author"
    "$.store.*"
    "$.store..price"
    "$..book[2]"
    "$..book[2].author"
    "$..book[2].publisher"
    "$..book[-1]"
    "$..book[0,1]"
    "$..book[:2]"
    "$..*"
    "$.a[*].b"
    "$.o['j j']"
    "$.o['j j']['k.k']"
    "$.o[\"j j\"][\"k.k\"]"
    "$[\"'\"][\"@\"]"
    "$.o[*,*]"
    "$[1:3]"
    "$[5:]"
    "$[1:5:2]"
    "$[5:1:-2]"
    "$[::-1]"
    "$[0:2,5]"
    "$[0,0]"
    "$..*"
    "$.o..[*,*]"
    "$.foo.*.bar"
  ]

  fun name(): String => "jsonpath/parser/rfc_examples"
  fun apply(h: TestHelper) =>
    let parser: Parser val = recover val JsonPathParser().eof() end
    for example in examples.values() do
      let source = Source.from_string(example)
      match parser.parse(source)
      | (let idx: USize, let p: Parser) => 
          h.fail("[FAILED] Query: '" + example + "': idx=" + idx.string())
      | (let idx: USize, let ast: ASTChild) =>
          h.log("[SUCCESS] Query: '" + example + "': idx=" + idx.string() + " ast=" + ast.label().text())
          let ast_str = recover val Printer(ast) end
          h.log(ast_str)

      end
    end

interface box _RFCExample
  fun tag name(): String
  fun tag input(): JsonType

  fun tag queries(): Array[(String, Array[JsonType] val)] val

primitive _ChildSegmentRFCExamples
  fun tag input(): JsonType => JsonArray([as JsonType: "a"; "b"; "c"; "d"; "e"; "f"; "g"])
  fun tag queries(): Array[(String, Array[JsonType] val)] val => [
    ("$[0, 3]",   [as JsonType: "a"; "d"])
    ("$[0:2, 5]", [as JsonType: "a"; "b"; "f"])
    ("$[0, 0]",   [as JsonType: "a"; "a"])
  ]
  fun tag name(): String => "child_segment"


class \nodoc\ iso _ExamplesTest is UnitTest
  let input: JsonType
  let queries: Array[(String, Array[JsonType] val)] val
  let _name: String

  new create(example: _RFCExample) =>
    input = example.input()
    queries = example.queries()
    _name = "jsonpath/rfc_examples/" + example.name()
  
  fun name(): String => _name

  fun apply(h: TestHelper) ? =>
    for (source, expected_output) in queries.values() do
      h.log("Compiling: '" + source + "' ...")
      let query = JsonPath.compile(source)?
      h.log("Query: '" + source + "' compiled successfully")
      let actual_output = recover val query.execute(input) end
      
      h.assert_eq[String](
        _JSON.s_array(expected_output),
        _JSON.s_array(actual_output),
        "Executing Query: '" + source + "' on input: " + _JSON.s(input))
    end

class _ArraySliceRFCExamples
  fun tag name(): String => "array_slice"
  fun tag input(): JsonType =>
    try
      _JSON.d("""["a", "b", "c", "d", "e", "f", "g"]""")?
    end
  fun tag queries(): Array[(String, Array[JsonType] val)] val => [
    ("$[1:3]", [as JsonType: "b"; "c"])
    ("$[5:]", [as JsonType: "f"; "g"])
    ("$[1:5:2]", [as JsonType: "b"; "d"])
    ("$[5:1:-2]", [as JsonType: "f"; "d"])
    ("$[::-1]", [as JsonType: "g"; "f"; "e"; "d"; "c"; "b"; "a"])
  ]

class _NullRFCExamples
  fun tag name(): String => "null"
  fun tag input(): JsonType =>
    try
      _JSON.d("""{"a": null, "b": [null], "c": [{}], "null": 1}""")?
    end
  fun tag queries(): Array[(String, Array[JsonType] val)] val => [
    ("$.a", [as JsonType: None])
    ("$.a[0]", [])
    ("$.a.d", [])
    ("$.b[0]", [as JsonType: None])
    ("$.b[*]", [as JsonType: None])
    ("$.null", [as JsonType: I64(1)])
  ]

class _DescendantSegmentRFCExamples
  fun tag name(): String => "descendant_segment"
  fun tag input(): JsonType =>
    try
      _JSON.d("""
      {
        "o": {"j": 1, "k": 2},
        "a": [5, 3, [{"j": 4}, {"k": 6}]]
      }
      """)?
    end
  fun tag queries(): Array[(String, Array[JsonType] val)] val =>
    [
      ("$..j",    [as JsonType: I64(1); I64(4)])
      ("$..[0]",  [as JsonType: I64(5); try _JSON.d("""{"j": 4}""")? end])
      ("$..*", [as JsonType: 
        try _JSON.d("""[5, 3, [{"j": 4}, {"k": 6}]]""")? end 
        try _JSON.d("""{"j": 1, "k": 2}""")? end 
        I64(5)
        I64(3)
        try _JSON.d("""[{"j": 4}, {"k": 6}]""")? end 
        I64(1)
        I64(2)
        try _JSON.d("""{"j": 4}""")? end 
        try _JSON.d("""{"k": 6}""")? end 
        I64(4)
        I64(6)
      ])
      ("$..o", [as JsonType: try _JSON.d("""{"j": 1, "k": 2}""")? end])
      ("$.o..[*,*]", [as JsonType: I64(1); I64(2); I64(1); I64(2)])
      ("$.a..[0,1]", [as JsonType: I64(5); I64(3); try _JSON.d("""{"j": 4}""")? end; try _JSON.d("""{"k": 6}""")? end])
    ]

primitive _MainRFCExamples
  fun tag name(): String => "main"
  fun tag input(): JsonType => 
    try 
      _JSON.d("""
      { "store": {
          "book": [
            { "category": "reference",
              "author": "Nigel Rees",
              "title": "Sayings of the Century",
              "price": 8.95
            },
            { "category": "fiction",
              "author": "Evelyn Waugh",
              "title": "Sword of Honour",
              "price": 12.99
            },
            { "category": "fiction",
              "author": "Herman Melville",
              "title": "Moby Dick",
              "isbn": "0-553-21311-3",
              "price": 8.99
            },
            { "category": "fiction",
              "author": "J. R. R. Tolkien",
              "title": "The Lord of the Rings",
              "isbn": "0-395-19395-8",
              "price": 22.99
            }
          ],
          "bicycle": {
            "color": "red",
            "price": 399
          }
        }
      }
      """)?
    end
  fun tag queries(): Array[(String, Array[JsonType] val)] val => [
    ("$.store.book[*].author", [as JsonType: "Nigel Rees"; "Evelyn Waugh"; "Herman Melville"; "J. R. R. Tolkien"])
    ("$..author",              [as JsonType: "Nigel Rees"; "Evelyn Waugh"; "Herman Melville"; "J. R. R. Tolkien"])
    ("$.store.*",              [as JsonType: try _JSON.d("""
    [
         { "category": "reference",
           "author": "Nigel Rees",
           "title": "Sayings of the Century",
           "price": 8.95
         },
         { "category": "fiction",
           "author": "Evelyn Waugh",
           "title": "Sword of Honour",
           "price": 12.99
         },
         { "category": "fiction",
           "author": "Herman Melville",
           "title": "Moby Dick",
           "isbn": "0-553-21311-3",
           "price": 8.99
         },
         { "category": "fiction",
           "author": "J. R. R. Tolkien",
           "title": "The Lord of the Rings",
           "isbn": "0-395-19395-8",
           "price": 22.99
         }
      ]""")? end
      try _JSON.d("""
      {
         "color": "red",
         "price": 399
      }
      """)? end
    ])
    ("$.store..price", [as JsonType: I64(399); F64(8.95); F64(12.99); F64(8.99); F64(22.99)])
    ("$..book[2]", [as JsonType: try _JSON.d("""
                      { "category": "fiction",
                        "author": "Herman Melville",
                        "title": "Moby Dick",
                        "isbn": "0-553-21311-3",
                        "price": 8.99
                      }""")? end])
    ("$..book[2].author", [as JsonType: "Herman Melville"])
    ("$..book[2].publisher", [])
    ("$..book[-1]", [as JsonType: try _JSON.d("""
                      { "category": "fiction",
                        "author": "J. R. R. Tolkien",
                        "title": "The Lord of the Rings",
                        "isbn": "0-395-19395-8",
                        "price": 22.99
                      }""")? end])
    ("$..book[0,1]", [as JsonType: 
      try _JSON.d("""
        { "category": "reference",
           "author": "Nigel Rees",
           "title": "Sayings of the Century",
           "price": 8.95
        }""")? end
      try _JSON.d("""
        { "category": "fiction",
           "author": "Evelyn Waugh",
           "title": "Sword of Honour",
           "price": 12.99
        }""")? end])
    ("$..book[:2]", [as JsonType: 
      try _JSON.d("""
        { "category": "reference",
           "author": "Nigel Rees",
           "title": "Sayings of the Century",
           "price": 8.95
        }""")? end
      try _JSON.d("""
        { "category": "fiction",
           "author": "Evelyn Waugh",
           "title": "Sword of Honour",
           "price": 12.99
        }""")? end])
    ("$..*", [as JsonType:
      try _JSON.d("""
        {
          "book": [
            { "category": "reference",
              "author": "Nigel Rees",
              "title": "Sayings of the Century",
              "price": 8.95
            },
            { "category": "fiction",
              "author": "Evelyn Waugh",
              "title": "Sword of Honour",
              "price": 12.99
            },
            { "category": "fiction",
              "author": "Herman Melville",
              "title": "Moby Dick",
              "isbn": "0-553-21311-3",
              "price": 8.99
            },
            { "category": "fiction",
              "author": "J. R. R. Tolkien",
              "title": "The Lord of the Rings",
              "isbn": "0-395-19395-8",
              "price": 22.99
            }
          ],
          "bicycle": {
            "color": "red",
            "price": 399
          }
        }
      """)? end
      try _JSON.d("""
        [
          { "category": "reference",
            "author": "Nigel Rees",
            "title": "Sayings of the Century",
            "price": 8.95
          },
          { "category": "fiction",
            "author": "Evelyn Waugh",
            "title": "Sword of Honour",
            "price": 12.99
          },
          { "category": "fiction",
            "author": "Herman Melville",
            "title": "Moby Dick",
            "isbn": "0-553-21311-3",
            "price": 8.99
          },
          { "category": "fiction",
            "author": "J. R. R. Tolkien",
            "title": "The Lord of the Rings",
            "isbn": "0-395-19395-8",
            "price": 22.99
          }
        ]
      """)? end
      try _JSON.d("""
        {
          "color": "red",
          "price": 399
        }
      """)? end
      try _JSON.d("""
          { "category": "reference",
            "author": "Nigel Rees",
            "title": "Sayings of the Century",
            "price": 8.95
          }
      """)? end
      try _JSON.d("""
          { "category": "fiction",
            "author": "Evelyn Waugh",
            "title": "Sword of Honour",
            "price": 12.99
          }
      """)? end
      try _JSON.d("""
          { "category": "fiction",
            "author": "Herman Melville",
            "title": "Moby Dick",
            "isbn": "0-553-21311-3",
            "price": 8.99
          }
      """)? end
      try _JSON.d("""
          { "category": "fiction",
            "author": "J. R. R. Tolkien",
            "title": "The Lord of the Rings",
            "isbn": "0-395-19395-8",
            "price": 22.99
          }
      """)? end
      I64(399)
      "red"
      F64(8.95)
      "Sayings of the Century"
      "reference"
      "Nigel Rees"
      F64(12.99)
      "Sword of Honour"
      "fiction"
      "Evelyn Waugh"
      F64(8.99)
      "0-553-21311-3"
      "Moby Dick"
      "fiction"
      "Herman Melville"
      F64(22.99)
      "0-395-19395-8"
      "The Lord of the Rings"
      "fiction"
      "J. R. R. Tolkien"
    ])
  ] 


primitive _JSON
  fun tag s_array(array: Array[JsonType] val): String =>
    _JSON.s(JsonArray(array))
  fun tag s(json: JsonType): String =>
    """serialize"""
    let doc = JsonDoc
    doc.data = json
    doc.string()

  fun tag d(input: String): JsonType ? =>
    "deserialize"
    JsonDoc .> parse(input)?.data
