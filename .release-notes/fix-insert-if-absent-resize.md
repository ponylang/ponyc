## Fix HashMap.insert_if_absent returning wrong value when insertion triggers resize

`insert_if_absent` returned the wrong value when inserting a new key caused the map to grow. The value was stored correctly — only the return value was wrong.
