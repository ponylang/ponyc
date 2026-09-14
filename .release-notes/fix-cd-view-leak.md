## Fix cycle detector memory leak for mutually-referencing actors

Long-running programs that repeatedly create and destroy pairs of actors holding references to each other leaked memory proportional to the number of pairs destroyed. A daemon accepting connections where each connection actor creates a handler that references it back leaked roughly 350 bytes per connection, with no ceiling.

Found and diagnosed by @In2infinity, who also provided a working fix that guided this change.
