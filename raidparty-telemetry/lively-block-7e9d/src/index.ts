export default {
	async fetch(request, env, ctx): Promise<Response> {
		const url = new URL(request.url);

		// GET /export — download all events as CSV
		if (request.method === "GET" && url.pathname === "/export") {
			if (!env.DOWNLOAD_KEY || url.searchParams.get("key") !== env.DOWNLOAD_KEY) {
				return new Response("Unauthorized", { status: 401 });
			}

			const db = env.raidparty_telemetry;
			const result = await db.prepare(
				"SELECT id, run_id, timestamp, type, data FROM events ORDER BY id"
			).all();

			let csv = "id,run_id,timestamp,type,data\n";
			for (const row of result.results) {
				const data = (row as any).data.replace(/"/g, '""');
				csv += `${(row as any).id},${(row as any).run_id},${(row as any).timestamp},${(row as any).type},"${data}"\n`;
			}

			return new Response(csv, {
				status: 200,
				headers: {
					"Content-Type": "text/csv",
					"Content-Disposition": "attachment; filename=raidparty_telemetry.csv",
				},
			});
		}

		// POST / — ingest telemetry data
		if (request.method === "POST") {
			const payload = await request.json() as any;
			const { run_id, events } = payload;
			if (!run_id || !events) {
				return new Response("Bad request", { status: 400 });
			}

			const db = env.raidparty_telemetry;

			for (const event of events) {
				const { type, ...data } = event;
				const timestamp = Math.floor(Date.now() / 1000);

				await db.prepare(
					"INSERT INTO events (run_id, timestamp, type, data) VALUES (?1, ?2, ?3, ?4)"
				).bind(run_id, timestamp, type, JSON.stringify(data)).run();
			}

			return new Response("OK", { status: 200 });
		}

		return new Response("Not found", { status: 404 });
	},
} satisfies ExportedHandler<Env>;
