FROM silkeh/clang:18

#COPY . /src/
#RUN mkdir /src/build
#WORKDIR /src/build
RUN apt update && apt install -y git
RUN apt install -y pip
RUN pip install --break-system-packages click
#RUN cmake -DCMAKE_BUILD_TYPE=Debug ..
#RUN cmake --build codegen -j
#./verify.py build -g --src ./targets/nonlinear_queue.cpp