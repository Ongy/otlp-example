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

void initTracer() {

  // CONFIGURE BATCH SPAN PROCESSOR PARAMETERS

  trace_sdk::BatchSpanProcessorOptions options{};

  // We export `kNumSpans` after every `schedule_delay_millis` milliseconds.
  options.max_export_batch_size = 50;
  // We make the queue size `KNumSpans`*2+5 because when the queue is half
  // full, a preemptive notif is sent to start an export call, which we want
  // to avoid in this simple example.
  options.max_queue_size = options.max_export_batch_size * 2 + 5;
  // Time interval (in ms) between two consecutive exports.
  options.schedule_delay_millis = std::chrono::milliseconds(3000);

  //	auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
  auto exporter = otlp::OtlpHttpExporterFactory::Create(opts);

  // auto processor =
  //     trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  auto processor = trace_sdk::BatchSpanProcessorFactory::Create(
      std::move(exporter), options);

  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(
          std::move(processor),
          opentelemetry::sdk::resource::Resource::Create(
              {{"service.name", "conntracker:conntrack"},
               {"service.namespace", "net.ongy"},
               {"process.executable.name", "conntrack"}}));

  // Set the global trace provider
  trace_api::Provider::SetTracerProvider(provider);
}

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

  // counter view
  std::unique_ptr<metric_sdk::InstrumentSelector> instrument_selector{
      new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kCounter,
                                         "name_counter")};
  std::unique_ptr<metric_sdk::MeterSelector> meter_selector{
      new metric_sdk::MeterSelector("Metric", "1.2",
                                    "https://opentelemetry.io/schemas/1.2.0")};
  std::unique_ptr<metric_sdk::View> sum_view{new metric_sdk::View{
      "Metric", "description", metric_sdk::AggregationType::kSum}};
  p->AddView(std::move(instrument_selector), std::move(meter_selector),
             std::move(sum_view));

  // histogram view
  std::string histogram_name = std::string("Metric") + "_histogram";
  std::unique_ptr<metric_sdk::InstrumentSelector> histogram_instrument_selector{
      new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kHistogram,
                                         "Histogram")};
  std::unique_ptr<metric_sdk::MeterSelector> histogram_meter_selector{
      new metric_sdk::MeterSelector("Metric", "1.2",
                                    "https://opentelemetry.io/schemas/1.2.0")};
  std::unique_ptr<metric_sdk::View> histogram_view{new metric_sdk::View{
      "Metric", "description", metric_sdk::AggregationType::kHistogram}};
  p->AddView(std::move(histogram_instrument_selector),
             std::move(histogram_meter_selector), std::move(histogram_view));

  // gauge view
  std::string updown_name = std::string("Metric") + "_updown";
  std::unique_ptr<metric_sdk::InstrumentSelector> updown_instrument_selector{
      new metric_sdk::InstrumentSelector(
          metric_sdk::InstrumentType::kUpDownCounter, "updown")};
  std::unique_ptr<metric_sdk::MeterSelector> updown_meter_selector{
      new metric_sdk::MeterSelector("Metric", "1.2",
                                    "https://opentelemetry.io/schemas/1.2.0")};
  std::unique_ptr<metric_sdk::View> updown_view{new metric_sdk::View{
      "Metric", "description", metric_sdk::AggregationType::kSum}};
  p->AddView(std::move(updown_instrument_selector),
             std::move(updown_meter_selector), std::move(updown_view));
}
} // namespace

using LongCounter = opentelemetry::v1::nostd::shared_ptr<
    opentelemetry::v1::metrics::Counter<long>>;

struct TrackerState {
  TrackerState() noexcept : updown_counter(nullptr) {}

  opentelemetry::v1::nostd::shared_ptr<
      opentelemetry::v1::metrics::UpDownCounter<long>>
      updown_counter;
  opentelemetry::v1::nostd::shared_ptr<
      opentelemetry::v1::metrics::Counter<long>>
      close_counter;

  struct {
    LongCounter direct;
    LongCounter delayed;
    LongCounter skipped;
  } persisted;
};

static void create_new_connection(TrackerState &state, uint64_t orig,
                                  uint64_t repl, struct nf_conntrack *ct) {
  auto provider = trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("conntracker", OPENTELEMETRY_SDK_VERSION);
  auto span = tracer->StartSpan(__FUNCTION__, {
                                                  {"srcIp", "127.0.0.1"},
                                                  {"dstIp", "8.8.4.4"},
                                                  {"srcPort", 12345},
                                                  {"dstPort", 53},
                                                  {"protocol", IPPROTO_TCP},
                                              });

  auto functionScope = trace::Scope(span);

  span->SetAttribute("ConnectionId", "UUID");

  state.persisted.direct->Add(1);
}

static void handle_connection_update(TrackerState &state, uint64_t orig,
                                     uint64_t repl, struct nf_conntrack *ct) {
  auto provider = trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("conntracker", OPENTELEMETRY_SDK_VERSION);

  auto functionSpan = trace::Scope(
      tracer->StartSpan(__FUNCTION__, {{"ConnectionID", "DUMMY"}}));
  /* We only ever have a reference in the tuple to id map, when
   * there is one in the id to state map as well! */

  if (rand() > RAND_MAX / 3) {
    if (rand() > RAND_MAX / 2) {
      state.persisted.delayed->Add(1);
    }
    state.persisted.direct->Add(1);
  } else {
    state.persisted.skipped->Add(1);
  }
}

/* TODO: This needs to be moved. Obviously */
static void handle_connection_new(struct nf_conntrack *ct, TrackerState &state,
                                  bool closing = false) {
  if (rand() > RAND_MAX / 19) {
    handle_connection_update(state, 0, 0, ct);
  } else {
    state.updown_counter->Add(1);
    create_new_connection(state, 0, 0, ct);
    if (closing) {
      state.close_counter->Add(1);
    }
  }
}

static void remove_connection_from_state(TrackerState &state) {
  auto provider = trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("conntracker", OPENTELEMETRY_SDK_VERSION);
  auto span = tracer->StartSpan(__FUNCTION__);
  auto functionSpan = trace::Scope(span);
}

static size_t msgs = 0;

static int print_new_conntrack(const struct nlmsghdr *hdr,
                               enum nf_conntrack_msg_type t,
                               struct nf_conntrack *ct, void *data) {
  ++msgs;
  auto provider = trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("conntracker", OPENTELEMETRY_SDK_VERSION);

  auto functionSpan = trace::Scope(tracer->StartSpan(__FUNCTION__));
  TrackerState *ptr = static_cast<TrackerState *>(data);
  auto &state = *ptr;

  if (rand() > RAND_MAX / 20) {
    handle_connection_new(ct, state);
  } else if (true) {
    handle_connection_new(ct, state);
	remove_connection_from_state(state);
    state.updown_counter->Add(-1);
  }

  return NFCT_CB_STOP;
}

int main(int argc, char **argv) {
  initTracer();
  initMetrics();

  TrackerState state = {};

  auto provider = metrics_api::Provider::GetMeterProvider();
  nostd::shared_ptr<metrics_api::Meter> meter =
      provider->GetMeter("Metric", "1.2.0");
  auto catch_counter = meter->CreateLongCounter("nfct_catch");
  auto close_counter = meter->CreateLongCounter("created_in_close");
  state.close_counter = close_counter;

  auto histogram_counter =
      meter->CreateLongHistogram("Histogram", "des", "unit");
  auto context = opentelemetry::context::Context{};

  state.persisted.delayed = meter->CreateLongCounter(
      "DelayedMetricReport", "Number of rx/tx metrics sent later with impact",
      "unit");

  state.persisted.direct = meter->CreateLongCounter(
      "DirectMetricReport",
      "Number of rx/tx metrics directly reported with impact", "unit");

  state.persisted.skipped = meter->CreateLongCounter(
      "SkippedMetricReport",
      "Number of rx/tx metrics skipped for no impact at first", "unit");

  auto updown_counter = meter->CreateLongUpDownCounter(
      "Connections", "Number of connections currently tracked", "unit");

  state.updown_counter = updown_counter;
  //	auto long_counter = meter->CreateLongObservableGauge(
  //	    "Connections", "Number of currently tracked connections", "unit");
  //	long_counter->AddCallback(MeasurementFetcher::Fetcher, &state);

  auto lambda = [&]() {
    catch_counter->Add(1);
    auto provider = trace::Provider::GetTracerProvider();
    auto tracer = provider->GetTracer("conntracker", OPENTELEMETRY_SDK_VERSION);

    auto functionSpan = trace::Scope(tracer->StartSpan("nfct_catch"));
    msgs = 0;
	size_t iterations = rand() % 1000;
    for (size_t i = 0; i < iterations; ++i) {
      print_new_conntrack(nullptr, NFCT_T_UNKNOWN, nullptr, &state);
    }

    histogram_counter->Record(msgs, context);

    return 0;
  };

  while (true) {
    lambda();
  }

  return 0;
}