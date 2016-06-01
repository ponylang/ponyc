struct val Product
  let id: U32
  let description: String

  new val create(id': U32, description': String) =>
    id = id'
    description = description'
