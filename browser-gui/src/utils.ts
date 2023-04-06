import { DraggableProvided, DraggableStateSnapshot } from 'react-beautiful-dnd'

export const draggableProps = (
  provided: DraggableProvided,
  snapshot: DraggableStateSnapshot,
) => ({
  ...provided.draggableProps,
  ...provided.dragHandleProps,
  style: {
    ...provided.draggableProps.style,
    ...(snapshot.isDropAnimating && {
      transitionDuration: '0.001s',
    }),
  },
})
