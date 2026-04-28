import json
import argparse
import os
import logging
import bisect
from collections import defaultdict
from typing import Tuple, Dict, Any, List

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")

def load_trace(file_path: str, auto_repair: bool = False) -> Tuple[Any, List[Dict]]:
    if not file_path or not os.path.exists(file_path):
        logging.error(f"File '{file_path}' does not exist.")
        return None, []

    logging.info(f"Loading '{file_path}' ...")
    with open(file_path, 'r', encoding='utf-8') as file_object:
        content = file_object.read().strip()

    if auto_repair and not content.endswith(']'):
        logging.info(f"Fixing missing closing bracket in '{file_path}'...")
        content = content[:content.rfind('}') + 1] + '\n]'

    try:
        data = json.loads(content)
        events = data.get("traceEvents", []) if isinstance(data, dict) else data
        return data, events
    except json.JSONDecodeError as error:
        logging.error(f"Failed to parse JSON: {error}")
        return None, []

def get_cann_pid(events: List[Dict]) -> int:
    return next((
        event.get("pid", 0) for event in events 
        if event.get("name") == "process_name" and event.get("args", {}).get("name") == "CANN"
    ), 0)

def merge_traces(profiler_path: str, custom_path: str, out_path: str, 
                 tolerance_us: float = 10000.0, search_window: int = 5):
    raw_data, profiler_events = load_trace(profiler_path)
    _, custom_events = load_trace(custom_path, auto_repair=True)

    if not profiler_events or not custom_events:
        logging.error("Failed to load events. Aborting merge.")
        return

    cann_pid = get_cann_pid(profiler_events)
    logging.info(f"Identified CANN PID: {cann_pid}")

    temp_map = defaultdict(list)
    for custom_event in custom_events:
        if custom_event.get("ph") == "X" and "args" in custom_event:
            temp_map[(custom_event.get("tid"), custom_event.get("name"))].append(
                (float(custom_event.get("ts", 0)), custom_event["args"])
            )

    custom_map = {}
    for key, values in temp_map.items():
        values.sort(key=lambda item: item[0])
        custom_map[key] = ([item[0] for item in values], [item[1] for item in values])

    logging.info(f"Built custom map with {len(custom_map)} unique keys.")

    need_be_matched_count = 0
    matched_count = 0
    used_custom_indices = set()

    for profiler_event in profiler_events:
        if profiler_event.get("ph") != "X" or profiler_event.get("pid") != cann_pid or not profiler_event.get("name"):
            continue

        event_name = profiler_event["name"]
        search_name = event_name.split("@")[1] if event_name.startswith("AscendCL@") else event_name
        key = (profiler_event.get("tid"), search_name)

        if key in custom_map:
            need_be_matched_count += 1
            profiler_timestamp = float(profiler_event.get("ts", 0))
            timestamps, args_list = custom_map[key]

            insert_index = bisect.bisect_right(timestamps, profiler_timestamp)
            best_args = None
            best_index = -1
            minimum_difference = float("inf")
            
            for index in range(max(0, insert_index - search_window), min(len(timestamps), insert_index + search_window)):
                time_difference = profiler_timestamp - timestamps[index]
                if 0 <= time_difference <= tolerance_us and time_difference < minimum_difference:
                    minimum_difference = time_difference
                    best_args = args_list[index]
                    best_index = (key[0], key[1], index)

            if best_args:
                if best_index in used_custom_indices:
                    logging.warning(f"Custom event at index {best_index} has already been matched. Skipping duplicate match.")
                    continue
                else:
                    used_custom_indices.add(best_index)

                profiler_args = profiler_event.setdefault("args", {})
                function_args = best_args.get("Function-Args", {})
                
                if "stream" in function_args:
                    profiler_args["Raw Stream"] = function_args["stream"]
                if "event" in function_args:
                    profiler_args["Event Id"] = function_args["event"]

                profiler_args.update(best_args)
                matched_count += 1

    if (need_be_matched_count != matched_count):
        logging.warning(f"Matched {matched_count} out of {need_be_matched_count} events. Some events may not have been merged correctly.")
    else:
        logging.info(f"Successfully matched and injected args into {matched_count} events.")
    logging.info(f"Saving merged trace to '{out_path}' ...")

    with open(out_path, 'w', encoding='utf-8') as output_file:
        json.dump(raw_data, output_file)
        
    logging.info("Merge complete! Drag it into https://ui.perfetto.dev to view.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Merge Prefill, Decode, and Custom Traces.")
    parser.add_argument("--profiler", required=True, help="Path to profiler trace json")
    parser.add_argument("--custom", required=True, help="Path to custom cpu hook trace json")
    parser.add_argument("--out", required=True, help="Path to output merged json")
    parser.add_argument("--tolerance", type=float, default=10000.0, help="Max time difference in microseconds")
    parser.add_argument("--window", type=int, default=5, help="Binary search neighbor window size")
    
    args = parser.parse_args()
    
    merge_traces(args.profiler, args.custom, args.out, args.tolerance, args.window)