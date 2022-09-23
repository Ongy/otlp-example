project(
    'conntracker',
    ['c', 'cpp'],
    version: '0.1',
    default_options: ['warning_level=3', 'c_std=c2x', 'cpp_std=c++20'],
)

cc = meson.get_compiler('cpp')

add_project_arguments('-Wmissing-declarations', language: 'c')
add_project_arguments('-Wmissing-declarations', language: 'cpp')

protobuf = dependency('protobuf')
otlp_api = dependency('opentelemetry-cpp',
  method: 'cmake',
  modules: [
    'opentelemetry-cpp::api',
    'opentelemetry-cpp::trace',
    'opentelemetry-cpp::proto',
    'opentelemetry-cpp::metrics',
    ])
otlp_sdk = dependency('opentelemetry-cpp',
  method: 'cmake',
  modules: [
    'opentelemetry-cpp::sdk',
    'opentelemetry-cpp::otlp_http_exporter',
    'opentelemetry-cpp::otlp_http_client',
    'opentelemetry-cpp::prometheus_exporter'
    ])

subdir('conntrack')
