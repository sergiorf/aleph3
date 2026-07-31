import { StrictMode, useEffect, useMemo, useState } from 'react';
import { createRoot } from 'react-dom/client';
import './styles.css';

type Diagnostic = {
  code: string;
  severity: string;
  message: string;
};

type EvaluationResult = {
  status: 'ok' | 'error';
  canonicalText: string | null;
  diagnostics: Diagnostic[];
};

type EvaluationResponse = {
  status: 'ok';
  sessionId: string;
  result: EvaluationResult;
};

type ApiError = {
  status: 'error';
  error: {
    code: string;
    message: string;
  };
};

const initialSource = '1/2 + 1/3';

async function readJson<T>(response: Response): Promise<T> {
  const body = await response.json();
  if (!response.ok) {
    const error = body as ApiError;
    throw new Error(error.error?.message ?? `Request failed with ${response.status}`);
  }
  return body as T;
}

function App() {
  const [source, setSource] = useState(initialSource);
  const [sessionId, setSessionId] = useState<string | null>(null);
  const [result, setResult] = useState<EvaluationResult | null>(null);
  const [status, setStatus] = useState<'idle' | 'starting' | 'running'>('starting');
  const [error, setError] = useState<string | null>(null);

  const canRun = useMemo(() => status === 'idle' && source.trim().length > 0, [source, status]);

  useEffect(() => {
    let cancelled = false;
    async function createSession() {
      try {
        const response = await fetch('/api/sessions', { method: 'POST' });
        const body = await readJson<{ sessionId: string }>(response);
        if (!cancelled) {
          setSessionId(body.sessionId);
          setStatus('idle');
        }
      } catch (requestError) {
        if (!cancelled) {
          setError(requestError instanceof Error ? requestError.message : 'Unable to create a session.');
          setStatus('idle');
        }
      }
    }
    createSession();
    return () => {
      cancelled = true;
    };
  }, []);

  async function runCell() {
    if (!sessionId || !canRun) {
      return;
    }
    setStatus('running');
    setError(null);
    try {
      const response = await fetch(`/api/sessions/${encodeURIComponent(sessionId)}/evaluate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ source })
      });
      const body = await readJson<EvaluationResponse>(response);
      setResult(body.result);
    } catch (requestError) {
      setError(requestError instanceof Error ? requestError.message : 'Evaluation failed.');
    } finally {
      setStatus('idle');
    }
  }

  return (
    <main className="workspace">
      <header className="topbar">
        <div>
          <h1>Aleph3</h1>
          <p>Symbolic notebook MVP</p>
        </div>
        <span className={sessionId ? 'service serviceReady' : 'service'}>
          {sessionId ? 'Session ready' : 'Starting session'}
        </span>
      </header>

      <section className="notebook" aria-label="Notebook evaluator">
        <div className="cellHeader">
          <span>Input</span>
          <button type="button" onClick={runCell} disabled={!canRun || !sessionId}>
            {status === 'running' ? 'Running' : 'Run'}
          </button>
        </div>
        <textarea
          value={source}
          onChange={(event) => setSource(event.target.value)}
          spellCheck={false}
          aria-label="Aleph3 expression input"
        />
        <div className="output" aria-live="polite">
          <span>Output</span>
          {result?.canonicalText ? <pre>{result.canonicalText}</pre> : <p className="muted">No output yet</p>}
        </div>
        {(error || (result?.diagnostics.length ?? 0) > 0) && (
          <div className="diagnostics">
            <span>Diagnostics</span>
            {error && <p>{error}</p>}
            {result?.diagnostics.map((diagnostic, index) => (
              <p key={`${diagnostic.code}-${index}`}>
                <strong>{diagnostic.code}</strong> {diagnostic.message}
              </p>
            ))}
          </div>
        )}
      </section>
    </main>
  );
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>
);
