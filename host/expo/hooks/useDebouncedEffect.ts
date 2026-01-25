import { useEffect, useRef } from "react";

/**
 * A hook that runs an effect after a debounce delay.
 * The effect will only run if the dependencies haven't changed for the specified delay.
 *
 * @param effect - The effect function to run (can return a cleanup function)
 * @param deps - Dependencies array (like useEffect)
 * @param delay - Debounce delay in milliseconds
 */
export function useDebouncedEffect(
  effect: () => void | (() => void),
  deps: React.DependencyList,
  delay: number,
): void {
  const cleanupRef = useRef<(() => void) | void>(void 0);
  const isFirstRender = useRef(true);

  useEffect(() => {
    // Skip the first render to avoid saving immediately on mount
    if (isFirstRender.current) {
      isFirstRender.current = false;
      return;
    }

    const timeoutId = setTimeout(() => {
      // Run any previous cleanup
      if (typeof cleanupRef.current === "function") {
        cleanupRef.current();
      }
      // Run the effect and store its cleanup
      cleanupRef.current = effect();
    }, delay);

    return () => {
      clearTimeout(timeoutId);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [...deps, delay]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (typeof cleanupRef.current === "function") {
        cleanupRef.current();
      }
    };
  }, []);
}
