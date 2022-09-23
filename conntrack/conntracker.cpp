#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#define PROJECT_NAME "conntracker"

extern "C" {
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <libnfnetlink/libnfnetlink.h>
#include <systemd/sd-bus.h>
}
extern "C" {
#include <time.h>
}

#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"

#include "opentelemetry/exporters/prometheus/exporter.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;

namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;

namespace metrics_api = opentelemetry::metrics;
namespace metric_sdk = opentelemetry::sdk::metrics;

namespace {
opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts = {
    .url = "http://mario.local.ongy.net:4318/v1/traces",
};

void initMetrics() {
  opentelemetry::exporter::metrics::PrometheusExporterOptions promOptions;
  metric_sdk::PeriodicExportingMetricReaderOptions options;

  options.export_interval_millis = std::chrono::milliseconds(1000);
  options.export_timeout_millis = std::chrono::milliseconds(500);

  promOptions.url = "0.0.0.0:9464";

  auto exporter =
      std::make_unique<opentelemetry::exporter::metrics::PrometheusExporter>(
          promOptions);
  std::unique_ptr<metric_sdk::MetricReader> reader{
      new metric_sdk::PeriodicExportingMetricReader(std::move(exporter),
                                                    options)};

  auto provider = std::shared_ptr<metrics_api::MeterProvider>(
      new metric_sdk::MeterProvider);
  metrics_api::Provider::SetMeterProvider(provider);

  auto p = std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
}
} // namespace

int main(void) {
  initMetrics();

  auto provider = metrics_api::Provider::GetMeterProvider();
  nostd::shared_ptr<metrics_api::Meter> meter =
      provider->GetMeter("Metric", "1.2.0");

  auto instrument = meter->CreateLongCounter(
      "DirectMetricReport",
      "Number of rx/tx metrics directly reported with impact", "unit");

  while (true) {
    instrument->Add(1);
  }

  return 0;
}