/** Central Action Flag Register */

// Action flag is an unsigned long bit field.
// This needs to be a bit field so that the corresponding actions a node can bind to can be easily checked against
// an action using logical AND.

#define PING_GRAPH_ACTION 0x00000001
/// Supports serialisation of an action, typically used for teleporting.
#define SERIALISABLE_GRAPH_ACTION 0x00000002
/// Supports invoking a script.
#define SCRIPT_GRAPH_ACTION 0x00000004
/// Supports building of a hive scene surface.
#define SCENE_GRAPH_ACTION 0x00000008
/// Supports strobing hive scene related nodes.
#define SCENE_STROBE_GRAPH_ACTION 0x00000010
/// Supports triggering and controlling animation.
#define ANIMATE_GRAPH_ACTION 0x00000020
/// Supports versioning a sub-graph.
#define VERSION_GRAPH_ACTION 0x00000040

//#define _GRAPH_ACTION 0x00000080

//#define _GRAPH_ACTION 0x00000100
//#define _GRAPH_ACTION 0x00000200
//#define _GRAPH_ACTION 0x00000400
//#define _GRAPH_ACTION 0x00000800

//#define _GRAPH_ACTION 0x00001000
//#define _GRAPH_ACTION 0x00002000
//#define _GRAPH_ACTION 0x00004000
//#define _GRAPH_ACTION 0x00008000

//#define _GRAPH_ACTION 0x00010000
//#define _GRAPH_ACTION 0x00020000
//#define _GRAPH_ACTION 0x00040000
//#define _GRAPH_ACTION 0x00080000

//#define _GRAPH_ACTION 0x00100000
//#define _GRAPH_ACTION 0x00200000
//#define _GRAPH_ACTION 0x00400000
//#define _GRAPH_ACTION 0x00800000

//#define _GRAPH_ACTION 0x01000000
//#define _GRAPH_ACTION 0x02000000
//#define _GRAPH_ACTION 0x04000000
//#define _GRAPH_ACTION 0x08000000

//#define _GRAPH_ACTION 0x10000000
//#define _GRAPH_ACTION 0x20000000
//#define _GRAPH_ACTION 0x40000000
//#define _GRAPH_ACTION 0x80000000
