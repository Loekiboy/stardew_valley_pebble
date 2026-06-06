import Poco from "commodetto/Poco";
import parseBMF from "commodetto/parseBMF";
import parseRLE from "commodetto/parseRLE";

const render = new Poco(screen);

// Custom font laden
function getFont(name, size) {
    const font = parseBMF(new Resource(`${name}-${size}.fnt`));
    font.bitmap = parseRLE(new Resource(`${name}-${size}-alpha.bm4`));
    return font;
}

const timeFont = getFont("svthin", 56);
const dateFont = getFont("svthin", 24);

// Kleuren
const black = render.makeColor(0, 0, 0);
const white = render.makeColor(255, 255, 255);

// Lay-out berekenen (één keer, niet elke frame)
const blockHeight = timeFont.height + dateFont.height;
const timeY = (render.height - blockHeight) / 2;
const dateY = timeY + timeFont.height;

const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

function draw(event) {
    const now = event.date;

    render.begin();
    render.fillRectangle(black, 0, 0, render.width, render.height);

    const hours = String(now.getHours()).padStart(2, "0");
    const minutes = String(now.getMinutes()).padStart(2, "0");
    const timeStr = `${hours}:${minutes}`;
    let width = render.getTextWidth(timeStr, timeFont);
    render.drawText(timeStr, timeFont, white,
        (render.width - width) / 2, timeY);

    const dayName = DAYS[now.getDay()];
    const monthName = MONTHS[now.getMonth()];
    const dateStr = `${dayName} ${monthName} ${String(now.getDate()).padStart(2, "0")}`;
    width = render.getTextWidth(dateStr, dateFont);
    render.drawText(dateStr, dateFont, white,
        (render.width - width) / 2, dateY);

    render.end();
}

watch.addEventListener("minutechange", draw);