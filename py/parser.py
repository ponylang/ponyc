import sys
import ply.yacc
import urllib.parse
from lexer import Lexer
from ast import AST

class ParseError(Exception): pass

class Parser(object):
  """
  A parser for the Pony language.
  """

  def __init__(self, optimize=False, debug=False, debuglevel=0):
    self.url = None
    self.lexer = Lexer(self._error_func)
    self.lexer.build(optimize = optimize)
    self.tokens = self.lexer.tokens
    self.debuglevel = debuglevel
    self.parser = ply.yacc.yacc(
      module=self,
      start='module',
      debug=debug,
      optimize=optimize,
      )

  def parse(self, url):
    """
    Currently only handles file URLs
    """
    self.url = url
    url = urllib.parse.urlparse(self.url)
    if url.scheme != 'file' or url.netloc != '':
      raise ParseError('Unable to load module: %s' % self.url)
    with open(url.path, 'rU') as f:
      text = f.read()
    self.lexer.reset()
    return self.parser.parse(text, lexer=self.lexer, debug=self.debuglevel)

  # Private

  def _error_func(self, line, col, msg):
    raise ParseError('%s [%s:%s]: %s' % (self.url, line, col, msg))

  # Grammar

  precedence = (
    ('left', 'OR'),
    ('left', 'XOR'),
    ('left', 'AND'),
    ('left', 'EQ', 'NE'),
    ('left', 'LT', 'LE', 'GE', 'GT'),
    ('left', 'LSHIFT', 'RSHIFT'),
    ('left', 'PLUS', 'MINUS'),
    ('left', 'TIMES', 'DIVIDE', 'MOD')
    )

  def p_module(self, p):
    """
    module : use_list type_def_list
    """
    p[0] = AST('module', [p[1], p[2]])

  def p_use_list(self, p):
    """
    use_list : use_list use
      | use
      | empty
    """
    if len(p) == 2:
      p[0] = AST('use_list', [p[1]])
    else:
      p[1].children.append(p[2])
      p[0] = p[1]

  def p_use(self, p):
    """
    use : USE STRING
      | USE ID EQUALS STRING
    """
    if len(p) == 2:
      p[0] = AST('use', [None, p[2]])
    else:
      p[0] = AST('use', [p[2], p[4]])

  def p_type_def_list(self, p):
    """
    type_def_list : type_def_list type_def
      | type_def_list type_decl
      | type_def
      | type_decl
      | empty
    """
    if len(p) == 2:
      p[0] = AST('type_def_list', [p[1]])
    else:
      p[1].children.append(p[2])
      p[0] = p[1]

  def p_type_def(self, p):
    """
    type_def : TYPE ID id_opt type_params_opt type_opt is
    """
    # FIX: remap methods
    if p[3] == None:
      p[0] = AST('type_def', [None, p[2], p[4], p[5], p[6]])
    else:
      p[0] = AST('type_def', [p[2], p[3], p[4], p[5], p[6]])

  def p_type_decl(self, p):
    """
    type_decl : ACTOR ID type_params_opt is LBRACE member_list RBRACE
      | CLASS ID type_params_opt is LBRACE member_list RBRACE
      | TRAIT ID type_params_opt is LBRACE member_list RBRACE
    """
    p[0] = AST(p[1], [p[2], p[3], p[4], p[6]])

  def p_is(self, p):
    """
    is : IS type_list
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_member_list(self, p):
    """
    member_list : member
      | member_list member
      | empty
    """
    if len(p) == 2:
      p[0] = AST('member_list', [p[1]])
    else:
      p[1].children.append(p[2])
      p[0] = p[1]

  def p_member(self, p):
    """
    member : VAR param
      | VAL param
      | method
    """
    if len(p) == 3:
      p[0] = AST(p[1], [p[2]])
    else:
      p[0] = p[1]

  def p_method(self, p):
    """
    method : PRIVATE def_or_msg ID type_params_opt params type body
      | def_or_msg ID type_params_opt params type body
    """
    if len(p) == 8:
      p[0] = AST('private', [p[2], p[3], p[4], p[5], p[6], p[7]])
    else:
      p[0] = AST('public', [p[1], p[2], p[3], p[4], p[5], p[6]])

  def p_def_or_msg(self, p):
    """
    def_or_msg : DEF
      | MSG
    """
    p[0] = p[1]

  def p_body(self, p):
    """
    body : EQUALS seq
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_type_list(self, p):
    """
    type_list : type
      | type_list COMMA type
    """
    if len(p) == 2:
      p[0] = AST('type_list', [p[1]])
    else:
      p[1].children.append(p[3])
      p[0] = p[1]

  def p_type(self, p):
    """
    type : base_type
      | type CASE base_type
    """
    if len(p) == 2:
      p[0] = AST('type', [p[1]])
    else:
      p[1].children.append(p[3])
      p[0] = p[1]

  def p_base_type(self, p):
    """
    base_type : ID id_opt type_args_opt
      | PARTIAL ID id_opt type_args_opt
      | params result
    """
    if len(p) == 4:
      if p[2] == None:
        p[0] = AST('base_type', [None, p[1], p[3]])
      else:
        p[0] = AST('base_type', [p[1], p[2], p[3]])
    elif len(p) == 5:
      if p[3] == None:
        p[0] = AST('partial_type', [None, p[2], p[4]])
      else:
        p[0] = AST('partial_type', [p[2], p[3], p[4]])
    else:
      p[0] = AST('lambda', [p[1], p[2]])

  def p_id_opt(self, p):
    """
    id_opt : DOT ID
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_type_params_opt(self, p):
    """
    type_params_opt : type_params
      | empty
    """
    p[0] = p[1]

  def p_type_params(self, p):
    """
    type_params : LBRACKET param_list RBRACKET
    """
    p[0] = p[2]

  def p_type_args_opt(self, p):
    """
    type_args_opt : type_args
      | empty
    """
    p[0] = p[1]

  def p_type_args(self, p):
    """
    type_args : LBRACKET arg_list RBRACKET
    """
    p[0] = p[2]

  def p_params(self, p):
    """
    params : LPAREN param_list RPAREN
      | LPAREN empty RPAREN
    """
    p[0] = p[2]

  def p_param_list(self, p):
    """
    param_list : param
      | param_list COMMA param
    """
    if len(p) == 2:
      p[0] = AST('params', [p[1]])
    else:
      p[1].children.append(p[3])
      p[0] = p[1]

  def p_param(self, p):
    """
    param : ID type_opt default_opt
    """
    p[0] = AST('param', [p[1], p[2], p[3]])

  def p_type_opt(self, p):
    """
    type_opt : COLON type
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_default_opt(self, p):
    """
    default_opt : EQUALS expr
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_result(self, p):
    """
    result : LPAREN type RPAREN
    """
    p[0] = p[2]

  def p_seq(self, p):
    """
    seq : expr
      | seq SEMI expr
    """
    if len(p) == 2:
      p[0] = AST('seq', [p[1]])
    else:
      p[1].children.append(p[3])
      p[0] = p[1]

  def p_expr_decl(self, p):
    """
    expr : VAR ID type_opt EQUALS expr
      | VAL ID type_opt EQUALS expr
    """
    p[0] = AST(p[1], [p[2], p[3], p[5]])

  def p_expr_assign(self, p):
    """
    expr : binary EQUALS expr
    """
    p[0] = AST('assign', [p[1], p[3]])

  def p_expr_stat(self, p):
    """
    expr : binary
    """
    p[0] = p[1]

  def p_expr_def(self, p):
    """
    expr : DEF type_params_opt params result EQUALS expr
    """
    p[0] = AST('def', [None, p[2], p[3], p[4], p[6]])

  def p_expr_if(self, p):
    """
    expr : IF seq THEN expr ELSE expr
    """
    p[0] = AST('if', [p[2], p[4], p[6]])

  def p_expr_while(self, p):
    """
    expr : WHILE seq DO expr
    """
    p[0] = AST('while', [p[2], p[4]])

  def p_expr_for(self, p):
    """
    expr : FOR ID IN seq DO expr
    """
    p[0] = AST('for', [p[2], p[4], p[6]])

  def p_expr_match(self, p):
    """
    expr : MATCH binary case_list
    """
    p[0] = AST('match', [p[2], p[3]])

  def p_case_list(self, p):
    """
    case_list : case
      | case_list case
    """
    if len(p) == 2:
      p[0] = AST('cases', [p[1]])
    else:
      p[1].children.append(p[2])
      p[0] = p[1]

  def p_case(self, p):
    """
    case : CASE compare as guard EQUALS binary
    """
    p[0] = AST('case', [p[2], p[3], p[4], p[6]])

  def p_compare(self, p):
    """
    compare : binary
      | empty
    """
    p[0] = p[1]

  def p_as(self, p):
    """
    as : AS ID COLON type
      | empty
    """
    if len(p) == 5:
      p[0] = AST('as', [p[2], p[4]])
    else:
      p[0] = p[1]

  def p_guard(self, p):
    """
    guard : IF binary
      | empty
    """
    if len(p) == 3:
      p[0] = p[2]
    else:
      p[0] = p[1]

  def p_binary(self, p):
    """
    binary : unary
      | binary binop unary
    """
    if len(p) == 2:
      p[0] = p[1]
    else:
      p[0] = AST('binop', [p[2], p[1], p[3]])

  def p_binop(self, p):
    """
    binop : PLUS
      | MINUS
      | TIMES
      | DIVIDE
      | MOD
      | LSHIFT
      | RSHIFT
      | LT
      | LE
      | EQ
      | NE
      | GE
      | GT
      | AND
      | OR
      | XOR
    """
    p[0] = p[1]

  def p_unary(self, p):
    """
    unary : postfix
      | unop unary
    """
    if len(p) == 2:
      p[0] = p[1]
    else:
      p[0] = AST('unop', [p[1], p[2]])

  def p_unop(self, p):
    """
    unop : NOT
      | MINUS
      | PARTIAL
    """
    p[0] = p[1]

  def p_postfix(self, p):
    """
    postfix : primary
      | postfix args
      | postfix DOT primary
    """
    if len(p) == 2:
      p[0] = p[1]
    elif len(p) == 3:
      p[0] = AST('call', [p[1], p[2]])
    else:
      p[0] = AST('access', [p[1], p[3]])

  def p_postfix2(self, p):
    """
    postfix : postfix type_args
    """
    p[0] = AST('type_args', [p[1], p[2]])

  def p_args(self, p):
    """
    args : LPAREN arg_list RPAREN
      | LPAREN empty RPAREN
    """
    p[0] = p[2]

  def p_arg_list(self, p):
    """
    arg_list : arg
      | arg_list COMMA arg
    """
    if len(p) == 2:
      p[0] = AST('args', [p[1]])
    else:
      p[1].children.append(p[3])
      p[0] = p[1]

  def p_arg(self, p):
    """
    arg : seq
      | ID EQUALS seq
    """
    if len(p) == 2:
      p[0] = p[1]
    else:
      p[0] = AST('optarg', [p[1], p[3]])

  def p_primary(self, p):
    """
    primary : THIS
      | TRUE
      | FALSE
      | INT
      | FLOAT
      | STRING
      | ID
      | REFLECT
      | ABSORB args
      | FIELD args
      | METHOD args
      | LBRACE seq RBRACE
    """
    if len(p) == 2:
      p[0] = AST('atom', [p[1]])
    elif len(p) == 3:
      p[0] = AST('refl', [p[1], p[2]])
    else:
      p[0] = AST('scope', [p[2]])

  def p_empty(self, p):
    """
    empty :
    """
    pass

  def p_error(self, p):
    if p == None:
      self._error_func('At end of input', '', 'Unexpected EOF')
    else:
      self._error_func(p.lineno, self.lexer.column(p), 'before: %s' % p.value)
