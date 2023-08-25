from mkvinfo import info
import sys, json

def getRawInfo(file):
    info_text = info(file)
    return json.loads(info_text)

def getTracksInfo(file):
    data = getRawInfo(file)
    data = data['Segment'][0]['Tracks'][0]['Track']

    for track in data:
        if "Video_track" in track:
            track['Video_track'] = track['Video_track'][0]

        if "Audio_track" in track:
            track['Audio_track'] = track['Audio_track'][0]

        if "Codecs_private_data" in track:
            del track['Codecs_private_data']

        if track["Track_type"] == "1":
            track["Track_type"] = "Video"

        if track["Track_type"] == "2":
            track["Track_type"] = "Audio"

        if track["Track_type"] == "17":
            track["Track_type"] = "Subtitle"

        track["Track_number"] = int(track["Track_number"]) - 1

    return data

print(json.dumps(getTracksInfo(sys.argv[1]), indent = 4))