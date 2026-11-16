import { readRepositoryFile } from '../lib/repository';

export function GET() {
	return new Response(readRepositoryFile('website/src/walkthrough/labrador.vsconfig'), {
		headers: {
			'Content-Type': 'application/json; charset=utf-8',
			'Content-Disposition': 'attachment; filename="labrador.vsconfig"',
		},
	});
}
