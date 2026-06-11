import "./style.css";
import { createJavaScriptRegexEngine } from "shiki/engine/javascript";
import { createHighlighterCore } from "shiki/core";
import cpp from "shiki/langs/cpp.mjs";
import githubLight from "shiki/themes/github-light.mjs";
import vectorSource from "../../../../include/chaistl/containers/sequence/vector.hpp?raw";
import vectorModifierTests from "../../../../test/chaistl/containers/sequence/vector/modifiers.cpp?raw";
import { vectorSourceMap } from "./sourceMap";
import { visualCases } from "./visualCases";

type TraceEvent = {
  id: number;
  op: string;
  label: string;
  detail: string;
  assertion: string;
  storage: string;
  fromSlot: number;
  toSlot: number;
  value: string;
  sourceFile: string;
  sourceAnchor: string;
  sourceLine: number;
};

type TraceDocument = {
  scenario: string;
  title?: string;
  coveredBy?: {
    file: string;
    test: string;
  };
  events: TraceEvent[];
};

type CellState = "raw" | "live" | "moved" | "released";

type Cell = {
  state: CellState;
  value: string;
};

type StorageBlock = {
  id: string;
  cells: Cell[];
  current: boolean;
};

type VisualState = {
  storages: Map<string, StorageBlock>;
  currentStorage: string | null;
};

type SourceDocument = {
  name: string;
  text: string;
};

type SourceRange = {
  start: number;
  end: number;
};

type WasmModule = {
  run_scenario(name: string): string;
};

type WasmFactory = (options?: {
  locateFile?: (path: string) => string;
}) => Promise<WasmModule>;

declare global {
  interface Window {
    createChaistlVisualizer?: WasmFactory;
  }
}

const app = document.querySelector<HTMLElement>("#app");

if (!app) {
  throw new Error("missing app root");
}

app.innerHTML = `
  <section class="shell">
    <header class="app-header">
      <div>
        <p class="eyebrow">chaistl visualizer</p>
        <h1>Vector storage trace</h1>
      </div>
      <div class="toolbar">
        <select id="scenario">
          ${visualCases
            .map((item) => `<option value="${item.scenario}">${item.title}</option>`)
            .join("")}
        </select>
        <button id="run">Run visual case</button>
      </div>
      <p id="status" class="status">Select a visual case and run it.</p>
    </header>
    <main class="debugger-layout">
      <section class="debug-sidebar" aria-label="Trace controls and memory state">
        <article class="panel step-panel">
          <div class="controls">
            <button id="reset" disabled>Reset</button>
            <button id="prev" disabled>Prev</button>
            <button id="next" disabled>Next</button>
            <span id="step" class="step">0 / 0</span>
          </div>
        </article>
        <article class="panel current-event-panel">
          <div class="panel-title">
            <h2>Current Event</h2>
            <span id="event-index">idle</span>
          </div>
          <div id="current-event" class="current-event">Run a visual case to start the trace.</div>
        </article>
        <article class="panel memory-panel">
          <div class="panel-title">
            <h2>Memory</h2>
            <span>storage blocks</span>
          </div>
          <div id="memory" class="memory"></div>
        </article>
        <article class="panel events-panel">
          <div class="panel-title">
            <h2>Events</h2>
            <span>current trace</span>
          </div>
          <ol id="events"></ol>
        </article>
        <details class="panel trace-panel">
          <summary>Trace JSON</summary>
          <pre id="json"></pre>
        </details>
      </section>
      <section class="source-pane" aria-label="Highlighted STL implementation source">
        <div class="panel-title">
          <h2>Source</h2>
          <span>test case and implementation</span>
        </div>
        <div class="source-grid">
          <article class="source-column">
            <div class="source-column-title">Test Case</div>
            <div id="test-source" class="source-view"></div>
          </article>
          <article class="source-column">
            <div class="source-column-title">Implementation</div>
            <div id="implementation-source" class="source-view"></div>
          </article>
        </div>
      </section>
    </main>
  </section>
`;

const status = document.querySelector<HTMLParagraphElement>("#status")!;
const runButton = document.querySelector<HTMLButtonElement>("#run")!;
const scenario = document.querySelector<HTMLSelectElement>("#scenario")!;
const eventList = document.querySelector<HTMLOListElement>("#events")!;
const jsonBlock = document.querySelector<HTMLPreElement>("#json")!;
const memory = document.querySelector<HTMLDivElement>("#memory")!;
const resetButton = document.querySelector<HTMLButtonElement>("#reset")!;
const prevButton = document.querySelector<HTMLButtonElement>("#prev")!;
const nextButton = document.querySelector<HTMLButtonElement>("#next")!;
const stepLabel = document.querySelector<HTMLSpanElement>("#step")!;
const testSourceView = document.querySelector<HTMLDivElement>("#test-source")!;
const implementationSourceView = document.querySelector<HTMLDivElement>("#implementation-source")!;
const eventIndex = document.querySelector<HTMLSpanElement>("#event-index")!;
const currentEventView = document.querySelector<HTMLDivElement>("#current-event")!;

let currentTrace: TraceDocument | null = null;
let currentStep = 0;

const sources: SourceDocument[] = [
  {
    name: "vector.hpp",
    text: vectorSource,
  },
  {
    name: "modifiers.cpp",
    text: vectorModifierTests,
  },
];

const highlighterPromise = createHighlighterCore({
  langs: [cpp],
  themes: [githubLight],
  engine: createJavaScriptRegexEngine(),
});

async function loadWasm(): Promise<WasmModule> {
  if (!window.createChaistlVisualizer) {
    await new Promise<void>((resolve, reject) => {
      const script = document.createElement("script");
      script.src = "/wasm/chaistl_visualizer.js";
      script.async = true;
      script.onload = () => resolve();
      script.onerror = () => reject(new Error("failed to load /wasm/chaistl_visualizer.js"));
      document.head.append(script);
    });
  }

  if (!window.createChaistlVisualizer) {
    throw new Error("chaistl_visualizer.js did not expose createChaistlVisualizer");
  }

  return window.createChaistlVisualizer({
    locateFile: (path: string) => `/wasm/${path}`,
  });
}

function emptyState(): VisualState {
  return {
    storages: new Map<string, StorageBlock>(),
    currentStorage: null,
  };
}

function parseValues(value: string): string[] {
  if (value.trim() === "") {
    return [];
  }
  return value.split(",").map((part) => part.trim());
}

function ensureStorage(state: VisualState, id: string, minSize: number): StorageBlock {
  const existing = state.storages.get(id);
  if (existing) {
    while (existing.cells.length < minSize) {
      existing.cells.push({ state: "raw", value: "" });
    }
    return existing;
  }

  const storage: StorageBlock = {
    id,
    cells: Array.from({ length: minSize }, () => ({ state: "raw", value: "" })),
    current: false,
  };
  state.storages.set(id, storage);
  return storage;
}

function firstOtherStorage(state: VisualState, id: string): StorageBlock | undefined {
  return [...state.storages.values()].find((storage) => storage.id !== id);
}

function applyEvent(state: VisualState, event: TraceEvent): void {
  switch (event.op) {
    case "snapshot": {
      const values = parseValues(event.value);
      const storage = ensureStorage(state, event.storage, values.length);
      storage.cells = values.map((value) => ({ state: "live", value }));
      storage.current = true;
      state.currentStorage = event.storage;
      break;
    }

    case "allocate": {
      const capacity = Number.parseInt(event.value, 10);
      const storage = ensureStorage(state, event.storage, Number.isFinite(capacity) ? capacity : 0);
      storage.cells = storage.cells.map(() => ({ state: "raw", value: "" }));
      storage.current = false;
      break;
    }

    case "construct": {
      const storage = ensureStorage(state, event.storage, event.toSlot + 1);
      storage.cells[event.toSlot] = { state: "live", value: event.value };
      break;
    }

    case "move_construct": {
      const target = ensureStorage(state, event.storage, event.toSlot + 1);
      const source = firstOtherStorage(state, event.storage) ?? target;
      const sourceValue = source.cells[event.fromSlot]?.value || event.value;
      target.cells[event.toSlot] = { state: "live", value: sourceValue };
      if (source.cells[event.fromSlot]) {
        source.cells[event.fromSlot] = { state: "moved", value: sourceValue };
      }
      break;
    }

    case "commit_storage": {
      for (const storage of state.storages.values()) {
        storage.current = storage.id === event.storage;
      }
      state.currentStorage = event.storage;
      break;
    }

    case "deallocate": {
      const storage = state.storages.get(event.storage);
      if (storage) {
        storage.cells = storage.cells.map((cell) => ({
          state: "released",
          value: cell.value,
        }));
        storage.current = false;
      }
      break;
    }

    default:
      break;
  }
}

function buildState(trace: TraceDocument, step: number): VisualState {
  const state = emptyState();
  for (const event of trace.events.slice(0, step)) {
    applyEvent(state, event);
  }
  return state;
}

function eventCellRole(
  state: VisualState,
  storage: StorageBlock,
  cellIndex: number,
  event: TraceEvent | undefined,
): string {
  if (!event) {
    return "";
  }

  if (event.storage === storage.id && event.toSlot === cellIndex) {
    return " target-cell";
  }

  if (event.op === "move_construct" && event.fromSlot === cellIndex) {
    const source = firstOtherStorage(state, event.storage);
    if (source?.id === storage.id) {
      return " source-cell";
    }
  }

  if ((event.op === "allocate" || event.op === "deallocate") && event.storage === storage.id) {
    return " storage-event-cell";
  }

  return "";
}

function renderCurrentEvent(event: TraceEvent | undefined): void {
  if (!event || !currentTrace) {
    eventIndex.textContent = "idle";
    currentEventView.textContent = "Run a visual case to start the trace.";
    return;
  }

  eventIndex.textContent = `${event.id + 1} / ${currentTrace.events.length}`;
  currentEventView.innerHTML = `
    <strong>${event.op}</strong>
    <span>${event.label}</span>
    ${event.detail ? `<p>${event.detail}</p>` : ""}
    <code>${event.storage}:${event.fromSlot} -> ${event.toSlot}</code>
    ${event.assertion ? `<small class="assertion">${event.assertion}</small>` : ""}
    ${
      currentTrace.coveredBy
        ? `<small>covered by ${currentTrace.coveredBy.test}</small>`
        : ""
    }
  `;
}

function renderMemory(state: VisualState, activeEvent: TraceEvent | undefined): void {
  if (state.storages.size === 0) {
    memory.textContent = "Run a visual case to inspect vector storage.";
    return;
  }

  memory.replaceChildren(
    ...[...state.storages.values()].map((storage) => {
      const block = document.createElement("section");
      block.className = `storage ${storage.current ? "current" : ""}`;

      const title = document.createElement("div");
      title.className = "storage-title";
      const liveCount = storage.cells.filter((cell) => cell.state === "live").length;
      title.textContent = `${storage.id} storage${storage.current ? " (current)" : ""} | size ${liveCount} / capacity ${
        storage.cells.length
      }`;

      const cells = document.createElement("div");
      cells.className = "cells";
      cells.replaceChildren(
        ...storage.cells.map((cell, index) => {
          const element = document.createElement("div");
          element.className = `cell ${cell.state}${eventCellRole(state, storage, index, activeEvent)}`;
          element.innerHTML = `
            <span class="slot">${index}</span>
            <strong>${cell.value || cell.state}</strong>
            <small>${cell.state}</small>
          `;
          return element;
        }),
      );

      block.append(title, cells);
      return block;
    }),
  );
}

function findLineIndex(lines: string[], pattern: string, fromIndex = 0): number {
  return lines.findIndex((line, lineIndex) => lineIndex >= fromIndex && line.includes(pattern));
}

function sourceRangeForEvent(source: SourceDocument, event: TraceEvent | undefined): SourceRange {
  if (!event) {
    return { start: 0, end: 0 };
  }

  if (source.name === "modifiers.cpp" && currentTrace?.coveredBy) {
    const lines = source.text.split("\n");
    const testStart = findLineIndex(lines, `TEST(VectorTest, ${currentTrace.coveredBy.test})`);
    if (testStart < 0) {
      return { start: 0, end: 0 };
    }

    const nextTest = lines.findIndex(
      (line, lineIndex) => lineIndex > testStart && line.startsWith("TEST("),
    );
    return {
      start: testStart + 1,
      end: nextTest > testStart ? nextTest : lines.length,
    };
  }

  if (event.sourceAnchor) {
    const lines = source.text.split("\n");
    const entry = vectorSourceMap.find((item) => item.anchor === event.sourceAnchor);

    if (entry) {
      const afterIndex = findLineIndex(lines, entry.after);
      const startIndex = findLineIndex(lines, entry.start, Math.max(0, afterIndex));
      if (startIndex >= 0) {
        const endIndex = entry.end ? findLineIndex(lines, entry.end, startIndex) : startIndex;
        return { start: startIndex + 1, end: Math.max(startIndex, endIndex) + 1 };
      }
    }
  }

  return { start: event.sourceLine, end: event.sourceLine };
}

async function renderSource(
  target: HTMLDivElement,
  source: SourceDocument,
  event: TraceEvent | undefined,
): Promise<void> {
  const highlightedRange = sourceRangeForEvent(source, event);
  const lines = source.text.split("\n");
  const highlighter = await highlighterPromise;
  const htmlLines = await Promise.all(
    lines.map((line, index) => {
      const html = highlighter.codeToHtml(line || " ", {
        lang: "cpp",
        theme: "github-light",
      });
      const body = html
        .replace(/^<pre[^>]*><code[^>]*>/, "")
        .replace(/<\/code><\/pre>$/, "");
      const lineNumber = index + 1;
      const active =
        lineNumber >= highlightedRange.start && lineNumber <= highlightedRange.end ? " active" : "";
      return `<div class="source-line${active}">
        <span class="line-number">${lineNumber}</span>
        <code>${body}</code>
      </div>`;
    }),
  );

  target.innerHTML = `
    <div class="source-title">
      ${source.name}${highlightedRange.start ? `:${highlightedRange.start}` : ""}${
        highlightedRange.end > highlightedRange.start ? `-${highlightedRange.end}` : ""
      }
      ${event?.sourceAnchor ? `<span>${event.sourceAnchor}</span>` : ""}
    </div>
    <div class="source-code">${htmlLines.join("")}</div>
  `;

  target.querySelector(".source-line.active")?.scrollIntoView({
    block: "center",
    inline: "nearest",
  });
}

function renderSources(event: TraceEvent | undefined): void {
  void renderSource(testSourceView, sources[1]!, event);
  void renderSource(implementationSourceView, sources[0]!, event);
}

function renderTrace(trace: TraceDocument): void {
  eventList.replaceChildren(
    ...trace.events.map((event) => {
      const item = document.createElement("li");
      item.dataset.eventId = String(event.id);
      item.innerHTML = `
        <strong>${event.op}</strong>
        <span>${event.label}</span>
        <code>${event.storage}:${event.fromSlot} -> ${event.toSlot}</code>
      `;
      return item;
    }),
  );
  jsonBlock.textContent = JSON.stringify(trace, null, 2);
}

function renderPlayer(): void {
  const trace = currentTrace;
  const total = trace?.events.length ?? 0;
  const activeEvent = trace?.events[currentStep - 1];
  const state = trace ? buildState(trace, currentStep) : emptyState();
  renderCurrentEvent(activeEvent);
  renderMemory(state, activeEvent);
  renderSources(activeEvent);

  stepLabel.textContent = `${currentStep} / ${total}`;
  resetButton.disabled = !trace || currentStep === 0;
  prevButton.disabled = !trace || currentStep === 0;
  nextButton.disabled = !trace || currentStep >= total;

  for (const item of eventList.querySelectorAll("li")) {
    item.classList.toggle("active", item.dataset.eventId === String(currentStep - 1));
  }
}

let wasm: WasmModule | null = null;

runButton.addEventListener("click", async () => {
  try {
    status.textContent = "Loading visual case...";
    const trace = visualCases.find((item) => item.scenario === scenario.value) as
      | TraceDocument
      | undefined;
    if (!trace) {
      throw new Error(`unknown visual case: ${scenario.value}`);
    }
    currentTrace = trace;
    currentStep = trace.events.length;
    renderTrace(trace);
    renderPlayer();
    status.textContent = `Loaded ${trace.events.length} steps from ${trace.coveredBy?.test ?? trace.scenario}.`;
  } catch (error) {
    status.textContent =
      error instanceof Error
        ? `Failed to load visual case: ${error.message}`
        : "Failed to load visual case.";
  }
});

resetButton.addEventListener("click", () => {
  currentStep = 0;
  renderPlayer();
});

prevButton.addEventListener("click", () => {
  currentStep = Math.max(0, currentStep - 1);
  renderPlayer();
});

nextButton.addEventListener("click", () => {
  currentStep = Math.min(currentTrace?.events.length ?? 0, currentStep + 1);
  renderPlayer();
});

renderPlayer();
