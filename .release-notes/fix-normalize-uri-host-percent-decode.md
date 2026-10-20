## Fix NormalizeURI not lowercasing characters decoded from percent-encoding in the host

`NormalizeURI` left decoded uppercase letters in the host un-lowered. `NormalizeURI` on `http://%41B.example/` produced `http://Ab.example/` instead of `http://ab.example/`, and normalizing the result again changed it, breaking idempotency. `URIEquivalent` reported `http://%41B.example/` and `http://ab.example/` as not equivalent.
