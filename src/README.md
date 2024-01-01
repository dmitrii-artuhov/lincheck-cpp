### Ltest

To build codegen pass:

* Clone and build LLVM according to the [guide](https://llvm.org/docs/GettingStarted.html).
* Run
  ```sh
  $ cmake -S. -Bbuild
  $ make -Cbuild
  ```

To run regression tests:

* Create virtual environment:
  ```sh
  $ python3 -m venv venv
  ```
* Install test dependencies:
  ```sh
  $ pip install -r ./test/regression/requirements.txt
  ```
* Run tests:
  ```
  $ pytest -v ./test/regression
  ```
