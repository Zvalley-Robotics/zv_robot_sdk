# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import math
import pathlib
import sys
import time

import zv_robot_sdk_python as zv


def parse_args():
    parser = argparse.ArgumentParser(
        description="ZV audio services and six-topic subscriber example"
    )
    parser.add_argument("--domain-id", type=int, default=0)
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--timeout", type=float, default=120.0)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping")

    synthesize = commands.add_parser("synthesize")
    synthesize.add_argument("text")
    synthesize.add_argument("--output-name", default="")
    synthesize.add_argument("--speaker-id", type=int, default=-1)
    synthesize.add_argument("--speaker", default="")

    play_stream = commands.add_parser("play-stream")
    play_stream.add_argument(
        "wav_file", type=pathlib.Path, help="WAV file on the SDK client host"
    )

    play_file = commands.add_parser("play-file")
    play_file.add_argument("file_path", help="WAV path on the robot/player host")
    watch = commands.add_parser("watch", help="subscribe to audio bridge topics")
    watch.add_argument("topic", choices=["all", "raw", "status", "result", "kws", "capture", "inference"])
    watch.add_argument("--duration", type=float, default=10.0)
    return parser.parse_args()


def watch_topics(args):
    if not math.isfinite(args.duration) or args.duration <= 0:
        print("duration must be positive and finite", file=sys.stderr)
        return 1
    subscriber = zv.AudioSubscriber()
    topics = {
        "raw": (zv.AudioTopic.Raw, subscriber.TakeRawAudio),
        "status": (zv.AudioTopic.SpeechStatus, subscriber.TakeSpeechStatus),
        "result": (zv.AudioTopic.SpeechResult, subscriber.TakeSpeechResult),
        "kws": (zv.AudioTopic.KwsTrigger, subscriber.TakeKwsTrigger),
        "capture": (zv.AudioTopic.CaptureDiagnostics, subscriber.TakeCaptureDiagnostics),
        "inference": (zv.AudioTopic.InferenceDiagnostics, subscriber.TakeInferenceDiagnostics),
    }
    selected = topics if args.topic == "all" else {args.topic: topics[args.topic]}
    subscriber.Init([topic for topic, _ in selected.values()])
    try:
        deadline = time.monotonic() + args.duration
        while time.monotonic() < deadline:
            for name, (_, take) in selected.items():
                available, event = take()
                if not available:
                    continue
                if name == "raw":
                    detail = f"samples={len(event.samples)}"
                elif name == "status":
                    detail = f"status={event.status}, wakeword={event.wakeword_id}"
                elif name == "result":
                    detail = f"text={event.text}, final={event.is_final}, backend={event.asr_mode}"
                elif name == "kws":
                    detail = event.data
                else:
                    detail = f"asr_state={event.asr_state}, frames={event.frames_processed}, error={event.error_msg}"
                print(f"{name}: sequence={event.sequence}, {detail}")
            time.sleep(0.01)
        for name, (topic, _) in selected.items():
            stats = subscriber.GetStats(topic)
            print(f"{name}: publishers={stats.matched_publishers}, received={stats.received}, "
                  f"invalid={stats.invalid}, dropped={stats.dropped}, pending={stats.pending}")
    finally:
        subscriber.Close()
    return 0


def print_playback_result(command, ret, result):
    output = sys.stdout if ret == zv.RPC_OK and result.success else sys.stderr
    print(
        f"{command}: rpc_code={ret}, success={result.success}, "
        f"sample_rate={result.sample_rate}, channels={result.channels}, "
        f"bits_per_sample={result.bits_per_sample}, encoding={result.encoding}, "
        f"frames_played={result.frames_played}, duration_s={result.duration_s}, "
        f"error={result.error_msg}",
        file=output,
    )
    return 0 if ret == zv.RPC_OK and result.success else 1


def main():
    args = parse_args()
    if not 0 <= args.domain_id <= 232 or not math.isfinite(args.timeout) or args.timeout <= 0:
        print("invalid domain ID or timeout", file=sys.stderr)
        return 1

    zv.ChannelFactory.Instance().Init(args.domain_id, args.network_interface)
    if args.command == "watch":
        return watch_topics(args)
    client = zv.AudioClient()
    client.Init()
    client.SetTimeout(args.timeout)

    if args.command == "ping":
        ret, result = client.Ping()
        print(result.message, file=sys.stdout if ret == zv.RPC_OK else sys.stderr)
        return 0 if ret == zv.RPC_OK else 1

    if args.command == "synthesize":
        if args.speaker_id < -1:
            print("speaker ID must be greater than or equal to -1", file=sys.stderr)
            return 1
        ret, result = client.SynthesizeSpeech(
            args.text, args.output_name, args.speaker_id, args.speaker
        )
        output = sys.stdout if ret == zv.RPC_OK and result.success else sys.stderr
        print(
            f"synthesize: rpc_code={ret}, success={result.success}, "
            f"file_path={result.file_path}, backend={result.backend}, "
            f"sample_rate={result.sample_rate}, duration_s={result.duration_s}, "
            f"rtf={result.rtf}, error={result.error_msg}",
            file=output,
        )
        return 0 if ret == zv.RPC_OK and result.success else 1

    if args.command == "play-stream":
        try:
            wav_data = args.wav_file.read_bytes()
        except OSError as error:
            print(f"failed to read WAV file: {error}", file=sys.stderr)
            return 1
        ret, result = client.PlayWavStream(wav_data)
    else:
        ret, result = client.PlayWavFile(args.file_path)

    return print_playback_result(args.command, ret, result)


if __name__ == "__main__":
    raise SystemExit(main())
