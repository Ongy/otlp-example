# Maintainer: Erik Zenker <erikzenker at posteo dot de>
pkgname=opentelemetry-cpp-git
pkgver=v1.6.1.r0.g5c180a168
pkgrel=1
pkgdesc="A microbenchmark support library, by Google"
arch=('i686' 'x86_64')
url="https://github.com/open-telemetry/opentelemetry-cpp"
license=('Apache')
depends=(gcc-libs gtest gmock)
makedepends=('cmake' benchmark-git)
options=(!strip debug)

source=("${pkgname}::git+https://github.com/open-telemetry/opentelemetry-cpp.git#tag=v1.6.1"
"prometheus-cpp::git+https://github.com/jupp0r/prometheus-cpp"
"vcpkg::git+https://github.com/Microsoft/vcpkg"
"ms-gsl::git+https://github.com/microsoft/GSL"
"gtest::git+https://github.com/google/googletest"
"gbench::git+https://github.com/google/benchmark"
"optlprot::git+https://github.com/open-telemetry/opentelemetry-proto"
"json::git+https://github.com/nlohmann/json"
"cive::git+https://github.com/civetweb/civetweb.git"
)

sha256sums=('SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP')

prepare() {
  cd "${srcdir}/${pkgname}"

  git submodule init
  git config submodule.third_party/prometheus-cpp.url "$srcdir/prometheus-cpp"
  git config submodule.tools/vcpkg.url "$srcdir/vcpkg"
  git config submodule.third_party/ms-gsl.url "$srcdir/ms-gsl"
  git config submodule.third_party/googletest.url "$srcdir/gtest"
  git config submodule.third_party/benchmark.url "$srcdir/gbench"
  git config submodule.third_party/opentelemetry-proto.url "$srcdir/optlprot"
  git config submodule.third_party/nlohmann-json.url "$srcdir/json"
  git submodule update

  git -C third_party/prometheus-cpp submodule init
  git -C third_party/prometheus-cpp config submodule.3rdparty/googletest.url "$srcdir/gtest"
  git -C third_party/prometheus-cpp config submodule.3rdparty/civetweb.url "$srcdir/cive"
  git -C third_party/prometheus-cpp submodule update

  mkdir -p build && cd build
  cmake .. -DCMAKE_BUILD_TYPE="Debug" \
           -DCMAKE_INSTALL_PREFIX=/usr \
           -DCMAKE_INSTALL_LIBDIR=lib \
           -DBUILD_SHARED_LIBS=ON \
           -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
           -DWITH_OTLP=ON \
           -DWITH_PROMETHEUS=ON
}

pkgver() {
  cd "$pkgname"
  git describe --long --tags | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd "${srcdir}/${pkgname}/build"
  cmake --build . --target all
}

check() {
  cd "${srcdir}/${pkgname}/build"
  ctest
}

package() {
  cd "${srcdir}/${pkgname}/build"
  make DESTDIR="$pkgdir/" install
}
