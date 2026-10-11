use "pony_compiler"

use @is_cap_sub_cap[Bool](
  sub: I32, subalias: I32, super_cap: I32, supalias: I32)

primitive CapSubtyping
  """
  Capability subtyping via the compiler's `is_cap_sub_cap`.
  """

  fun is_sub(sub: String val, super_cap: String val): Bool =>
    """
    True when `sub` is a subtype of `super_cap`. Both are capability
    names ("iso", "trn", "ref", "val", "box", "tag"). Returns false
    for unrecognized names.
    """
    let sub_id = _cap_token_id(sub)
    let super_id = _cap_token_id(super_cap)
    if (sub_id == TokenIds.tk_none()) or (super_id == TokenIds.tk_none()) then
      return false
    end
    @is_cap_sub_cap(sub_id, TokenIds.tk_none(), super_id, TokenIds.tk_none())

  fun is_sub_ephemeral(
    sub: String val,
    sub_ephemeral: Bool,
    super_cap: String val,
    super_ephemeral: Bool)
    : Bool
  =>
    """
    Subtype check with ephemeral modifiers. An ephemeral capability
    (`iso^`, `trn^`) can become more types than its non-ephemeral
    counterpart.
    """
    let sub_id = _cap_token_id(sub)
    let super_id = _cap_token_id(super_cap)
    if (sub_id == TokenIds.tk_none()) or (super_id == TokenIds.tk_none()) then
      return false
    end
    let sub_eph =
      if sub_ephemeral then TokenIds.tk_ephemeral()
      else TokenIds.tk_none()
      end
    let super_eph =
      if super_ephemeral then TokenIds.tk_ephemeral()
      else TokenIds.tk_none()
      end
    @is_cap_sub_cap(sub_id, sub_eph, super_id, super_eph)

  fun cap_string(token_id: TokenId): String val =>
    """
    The capability name for a token ID, or "" for non-capability
    tokens.
    """
    match token_id
    | TokenIds.tk_iso() => "iso"
    | TokenIds.tk_trn() => "trn"
    | TokenIds.tk_ref() => "ref"
    | TokenIds.tk_val() => "val"
    | TokenIds.tk_box() => "box"
    | TokenIds.tk_tag() => "tag"
    else ""
    end

  fun _cap_token_id(name: String val): TokenId =>
    match name
    | "iso" => TokenIds.tk_iso()
    | "trn" => TokenIds.tk_trn()
    | "ref" => TokenIds.tk_ref()
    | "val" => TokenIds.tk_val()
    | "box" => TokenIds.tk_box()
    | "tag" => TokenIds.tk_tag()
    else TokenIds.tk_none()
    end
