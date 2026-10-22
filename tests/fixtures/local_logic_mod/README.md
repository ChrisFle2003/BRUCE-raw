# Local Logic Mod builder fixture

The Logic builder tests assemble their package in a private temporary directory
from the checked-in signed Data-Mod fixture and the prebuilt test worker target.
No private operator key, compiler invocation, or generated package is stored
here. The signing material is the public RFC 8032 test vector used throughout
the runtime-mod contract tests.
