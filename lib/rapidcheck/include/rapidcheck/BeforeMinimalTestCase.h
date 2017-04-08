#pragma once

namespace rc {

/// This is called before the final minimal test case is run when a property
/// fails. Set a breakpoint here if you want to debug that test case. When the
/// debugger breaks here, you can set up any further breakpoints or other tools
/// before the test case is actually run.
void beforeMinimalTestCase();

} // namespace rc
