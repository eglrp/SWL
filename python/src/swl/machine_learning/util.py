import numpy as np

def to_one_hot_encoding(label_indexes, num_classes=None):
	if None == num_classes:
		num_classes = np.max(label_indexes) + 1
	elif num_classes <= np.max(label_indexes):
		raise ValueError('num_classes has to be greater than np.max(label_indexes)')
	return np.eye(num_classes)[label_indexes]
	#return np.transpose(np.eye(num_classes)[label_indexes])
