## Add distributed cycle detection mode

`--ponydistributedcd` enables an alternative cycle detection mode where actors detect cycles among themselves via message passing. Each blocked actor traces its outgoing references and sends what it finds to the actors it can reach. When a group of actors forms a cycle and each member's queue is empty, one member sends destruction messages to the group.

The centralized detector (the default) maintains a global view of all actor references from a single coordinating actor. The distributed mode eliminates that central coordinator — detection runs across the actors involved in cycles.

The flag is mutually exclusive with `--ponynoblock`. Like `--ponynoblock`, actors collected by the distributed protocol are not finalized at shutdown.
