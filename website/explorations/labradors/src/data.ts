export const directions = [
	{
		id: 'dry',
		number: '01',
		name: 'Dry sibling aside',
		short: 'A prepared demo. An unprepared entrance.',
		description: 'Ani claims the presentation, then admits the entrance was his entire plan. A small interruption after the real sample introduction.',
		role: 'Based on Matthew’s attention-stealing example. These lines, including Vader’s reply, are invented.',
		placement: 'Candidate: one aside beside the Home demo introduction. Never on technical reference pages.',
		home: [
			{ dog: 'vader', text: 'I brought a demo.' },
			{ dog: 'ani', text: 'I’m presenting.' },
			{ dog: 'vader', text: 'What have you prepared?' },
			{ dog: 'ani', text: 'An entrance.' },
		],
		examples: [
			{ dog: 'vader', text: 'I saved you a place.' },
			{ dog: 'ani', text: 'The best one?' },
			{ dog: 'vader', text: 'Right next to me.' },
			{ dog: 'ani', text: 'That’ll do.' },
		],
	},
	{
		id: 'duo',
		number: '02',
		name: 'Playful interruption',
		short: 'Vader has his moment. Ani wants in.',
		description: 'Vader is excited to show Mum something. Ani takes over, without anything to show except himself. The invitation at the end keeps the affection visible.',
		role: 'Vader has his own enthusiasm and soft spot for Mum. Ani wants the attention more than the toy; neither is reduced to a stock good dog or bad dog.',
		placement: 'Candidate: beside an Examples preview, after the real sample links. Never on technical reference pages.',
		home: [
			{ dog: 'vader', text: 'Mum’s going to love this.' },
			{ dog: 'ani', text: 'I’m showing her.' },
			{ dog: 'vader', text: 'What are you showing?' },
			{ dog: 'ani', text: 'Me. You can come too.' },
		],
		examples: [
			{ dog: 'vader', text: 'Watch this bit.' },
			{ dog: 'ani', text: 'Watch me.' },
			{ dog: 'vader', text: 'Show us, then.' },
			{ dog: 'ani', text: '…Vader?' },
		],
	},
	{
		id: 'guides',
		number: '03',
		name: 'Warm sibling guide',
		short: 'Ani insists on leading. With Vader nearby.',
		description: 'Vader offers to help. Ani claims the job, then lets the affection show. A gentler option for the route into Examples.',
		role: 'Vader’s care and Ani’s difficulty being apart come from Matthew’s account. Their imagined conversation is a creative proposal.',
		placement: 'Candidate: Home’s link into Examples. No instructions depend on the dialogue. Never on technical reference pages.',
		home: [
			{ dog: 'vader', text: 'I’ll show you around.' },
			{ dog: 'ani', text: 'I’ll do it.' },
			{ dog: 'vader', text: 'After you.' },
			{ dog: 'ani', text: 'Only if you’re coming.' },
		],
		examples: [
			{ dog: 'vader', text: 'I’m going to practise.' },
			{ dog: 'ani', text: 'My turn!' },
			{ dog: 'vader', text: 'You want to practise?' },
			{ dog: 'ani', text: 'No. But I want a turn.' },
			{ dog: 'vader', text: 'Come on, then. Beside me.' },
		],
	},
] as const;

export type Direction = (typeof directions)[number];
