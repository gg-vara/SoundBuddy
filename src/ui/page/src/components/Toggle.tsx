// On / off switch, controlled : it shows "checked", and asks for the change through "onChange"
import type { JSX } from '@solidjs/web';

export function Toggle(props: { checked: boolean; onChange: (checked: boolean) => void; children: JSX.Element }) {
  return (
    <label class="switch">
      <input
        type="checkbox"
        checked={props.checked}
        onChange={event => {
          const wanted = event.currentTarget.checked;
          // stays as the application says, until it confirms
          event.currentTarget.checked = props.checked;
          props.onChange(wanted);
        }}
      />
      <span class="track">
        <span class="thumb" />
      </span>
      <span>{props.children}</span>
    </label>
  );
}
