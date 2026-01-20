primitive PegParser
  fun apply(): Parser val =>
    recover
      let digit = R('0', '9')
      let hex = digit / R('a', 'f') / R('A', 'F')
      let char =
        L("\\0") / L("\\\"") / L("\\\\") /
        L("\\a") / L("\\b") / L("\\f") / L("\\n") /
        L("\\r") / L("\\t") / L("\\v") / L("\\'") / 
        (L("\\x") * hex * hex) /
        (L("\\u") * hex * hex * hex * hex) /
        (L("\\U") * hex * hex * hex * hex * hex * hex) /
        (not L("\"") * not L("\\") * R(' '))

      let string = (L("\"") * char.many() * L("\"")).term(PegString)
      let charlit = (L("'") * char * L("'")).term(PegChar)
      let dot = L(".").term(PegAny)

      let ident_start = R('a', 'z') / R('A', 'Z') / L("_")
      let ident_cont = ident_start / R('0', '9')
      let ident = (ident_start * ident_cont.many()).term(PegIdent)
      let range = (charlit * -L("..") * charlit).node(PegRange)

      let expr = Forward
      let group = -L("(") * expr * -L(")")
      let primary =
        (ident * not L("<-")) / group / range / string / charlit / dot
      let suffix =
        (primary * -L("?")).node(PegOpt) /
        (primary * -L("*")).node(PegMany) /
        (primary * -L("+")).node(PegMany1) /
        (primary * -L("%+") * primary).node(PegSep1) /
        (primary * -L("%") * primary).node(PegSep) /
        primary
      let prefix =
        (-L("&") * suffix).node(PegAnd) /
        (-L("!") * suffix).node(PegNot) /
        (-L("-") * suffix).node(PegSkip) /
        suffix
      let sequence = prefix.many1().node(PegSeq)
      expr() = sequence.many1(L("/")).node(PegChoice)
      let definition = (ident * -L("<-") * expr).node(PegDef)

      let whitespace = (L(" ") / L("\t") / L("\r") / L("\n")).many1()
      let linecomment =
        (L("#") / L("//")) * (not L("\r") * not L("\n") * Unicode).many()
      let nestedcomment = Forward
      nestedcomment() =
        L("/*") *
        ((not L("/*") * not L("*/") * Unicode) / nestedcomment).many() *
        L("*/")
      let hidden = (whitespace / linecomment / nestedcomment).many()
      definition.many1().node(PegGrammar).hide(hidden)
    end

primitive PegString is Label fun text(): String => "String"
primitive PegChar is Label fun text(): String => "Char"
primitive PegAny is Label fun text(): String => "Any"
primitive PegIdent is Label fun text(): String => "Ident"
primitive PegRange is Label fun text(): String => "Range"
primitive PegOpt is Label fun text(): String => "Opt"
primitive PegMany is Label fun text(): String => "Many"
primitive PegMany1 is Label fun text(): String => "Many1"
primitive PegSep is Label fun text(): String => "Sep"
primitive PegSep1 is Label fun text(): String => "Sep1"
primitive PegAnd is Label fun text(): String => "And"
primitive PegNot is Label fun text(): String => "Not"
primitive PegSkip is Label fun text(): String => "Skip"
primitive PegSeq is Label fun text(): String => "Seq"
primitive PegChoice is Label fun text(): String => "Choice"
primitive PegDef is Label fun text(): String => "Def"
primitive PegGrammar is Label fun text(): String => "Grammar"
