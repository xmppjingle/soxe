-module(soxe_tests).
-compile([warnings_as_errors, debug_info]).
-author('manuel@altenwald.com').

-include_lib("eunit/include/eunit.hrl").
-include("soxe.hrl").

-define(AUDIO_WAV, "test/alesis_acoustic_bass.wav").
-define(AUDIO_GSM, "test/alesis_acoustic_bass.gsm").

-define(AUDIO_SILENCE,      "test/totrim.wav").
-define(AUDIO_SILENCE_TRIM, "test/trimmed.wav").

info_test() ->
    ok = soxe:start(),
    Info = #soxe_info{ filename = ?AUDIO_WAV,
                       bitrate = 44100,
                       channels = 2,
                       encoding_text = "Signed PCM",
                       encoding_id = 1,
                       audio_length = 255564,
                       duration = 2 },
    ?assertEqual(Info, soxe:info(?AUDIO_WAV)),
    ok = soxe:stop(),
    ok.

convert_test() ->
    ok = soxe:start(),
    ok = soxe:convert(?AUDIO_WAV, ?AUDIO_GSM),
    Info = #soxe_info{ filename = ?AUDIO_GSM,
                       bitrate = 8000,
                       channels = 1,
                       encoding_text = "GSM",
                       encoding_id = 21,
                       audio_length = 0,
                       duration = 0 },
    ?assertEqual(Info, soxe:info(?AUDIO_GSM)),
    ok = soxe:stop(),
    ok.

trim_silence_test() ->
    ok = soxe:start(),
    ok = soxe:trim_silence(?AUDIO_SILENCE, ?AUDIO_SILENCE_TRIM, "0.1", "0.4%"),
    % soxe:trim_silence("w" ++ ?AUDIO_SILENCE, "w" ++ ?AUDIO_SILENCE_TRIM, 0, "1", "0.1", "0.3"),
    ok = soxe:stop(),
    ok.
