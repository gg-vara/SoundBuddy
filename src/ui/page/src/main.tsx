// Entry point : the page's state machine, fed by the application, shown by <App>
import { render } from '@solidjs/web';
import { createActor } from 'xstate';
import { bouncy, snappy } from './animations';
import { App } from './App';
import { texts } from './i18n';
import { pageMachine } from './machine';
import { hostNative, listenToNative, type Native } from './native';
import { createView } from './view';
import './style.css';

async function native(): Promise<Native> {
  const host = hostNative();
  if (host) return host;

  // in a browser (bun run dev) : a stand-in for the application, left out of builds
  if (import.meta.env.DEV) return (await import('./dev/fakeNative')).fakeNative();
  throw new Error('No application hosts this page');
}

// springs, for the CSS transitions of style.css
const root = document.documentElement.style;
root.setProperty('--bouncy', bouncy.easing);
root.setProperty('--bouncy-duration', `${bouncy.duration}ms`);
root.setProperty('--snappy', snappy.easing);
root.setProperty('--snappy-duration', `${snappy.duration}ms`);

const page = createActor(pageMachine, { input: { native: await native(), texts } });
const view = createView(page);
listenToNative(event => page.send({ type: 'native.event', event }));
page.start();

render(() => <App view={view} />, document.getElementById('root')!);
