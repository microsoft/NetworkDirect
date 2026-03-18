# NetworkDirect SPI

This fork contains updates that allow building everything with Visual Studio 2022. There is no dependency on CBT (common build tools).

The NetworkDirect architecture provides application developers with a networking interface that enables zero-copy data transfers between applications, kernel-bypass I/O generation and completion processing, and one-sided data transfer operations. The NetworkDirect service provider interface (SPI) defines the interface that NetworkDirect providers implement to expose their hardware capabilities to applications.

Please find additional documentation in [docs/](./docs) folder.

NetworkDirect SDK is available in [Nuget](https://www.nuget.org/packages/networkdirect) also.

# Building

## Prerequisites

 - [Visial Studio 2022](https://visualstudio.microsoft.com/vs/)

   Please make sure to select the following workloads during installation:
    - Desktop development with C++ 

 ## Build
 To build, open a __Developer Command Prompt for VS 2022__ and  run ``msbuild`` from src folder.

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
